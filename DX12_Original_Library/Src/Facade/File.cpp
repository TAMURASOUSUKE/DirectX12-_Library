#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <windows.h>
#include <system_error>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string_view>
#include <bcrypt.h>
#include <algorithm>
#include "../Core/TextEncoding.h"
#include "../Debug/DebugLogs.h"
#include "File.h"

#pragma comment(lib, "bcrypt.lib")

namespace
{
	// CSVの現在のフィールドをどの状態で読んでいるか
	enum class CSVFieldState
	{
		Unquoted, // 引用符で囲まれていない通常状態
		Quoted, // "..."の内側
		QuoteClosed // 閉じる引用符を読んだ直後
	};

	// ファイルの先頭に保存する識別子
	constexpr std::array<std::uint8_t, 4> ENCRYPTED_FILE_MAGIC{ 'T', 'S', 'G', 'F' };
	// 現在の暗号ファイル形式
	constexpr std::uint8_t ENCRYPTED_FILE_VERSION{ 1 };
	// 暗号方式を識別する番号
	constexpr std::uint8_t ENCRYPTION_AES_256_GCM{ 1 };
	// GCMで一般に使用される96bitのNonce
	constexpr std::size_t GCM_NONCE_SIZE{ 12 };
	// 改ざん検知に使用する128bitのタグ
	constexpr std::size_t GCM_TAG_SIZE{ 16 };
	// magic, version, algorithm, reserved, datasizeを合わせたサイズ
	constexpr std::size_t ENCRYPTED_HEADER_SIZE{ 16 };
	// NonceとTagを含めた暗号文本体の開始位置
	constexpr std::size_t ENCRYPTED_PAYLOAD_OFFSET{ ENCRYPTED_HEADER_SIZE + GCM_NONCE_SIZE + GCM_TAG_SIZE };
}

namespace
{
	// UTF-8文字列をCSVの行と列へ分解する
	bool ParseCSVText(std::string_view _text, std::vector<std::vector<std::string>>& _outRecords, std::size_t& _outErrorLine)
	{
		// 解析途中で失敗しても出力を壊さないように一時領域を使う
		std::vector<std::vector<std::string>> records{};
		std::vector<std::string> currentRow{};
		std::string currentField{};

		CSVFieldState state{ CSVFieldState::Unquoted };
		std::size_t currentLine{ 1 };

		// 空行と「空文字列を持つ行」を区別するためのフラグ
		bool recordTouched{ false };

		// 現在のfieldを現在行へと追加する
		auto finishField = [&]()
		{
			currentRow.push_back(std::move(currentField));
			currentField.clear();
			state = CSVFieldState::Unquoted;
		};

		// 現在行をレコードとして追加する
		auto finishRecord = [&]()
		{
			finishField();
			// 完全な空行は無視「,,」のような行はrecordTouchedがtrueなので残る
			if (recordTouched) records.push_back(std::move(currentRow));
			currentRow.clear();
			recordTouched = false;
		};

		for (std::size_t i = 0; i < _text.size(); i++)
		{
			const char currentChar{ _text[i] };

			switch (state)
			{
			case CSVFieldState::Unquoted:
				if (currentChar == '"')
				{
					// 通常文字を読んだ後から引用符を開始する形式は不正とする
					if (!currentField.empty())
					{
						_outErrorLine = currentLine;
						return false;
					}
					state = CSVFieldState::Quoted;
					recordTouched = true;
				}
				else if (currentChar == ',')
				{
					// カンマで現在の列を確定する
					finishField();
					recordTouched = true;
				}
				else if (currentChar == '\r' || currentChar == '\n')
				{
					// CRLFは2文字で1改行なので、LF側も同時に読み飛ばす
					if (currentChar == '\r' && i + 1 < _text.size() && _text[i + 1] == '\n') i++;

					finishRecord();
					currentLine++;
				}
				else
				{
					currentField.push_back(currentChar);
					recordTouched = true;
				}
				break;

			case CSVFieldState::Quoted:
				if (currentChar == '"')
				{
					// 引用符内の""は、文字としての"を表す
					if (i + 1 < _text.size() && _text[i + 1] == '"')
					{
						currentField.push_back('"');
						i++;
					}
					else
					{
						// 単独の引用符ならフィールドを閉じる
						state = CSVFieldState::QuoteClosed;
					}
				}
				else
				{
					// 引用符内ではカンマや改行もデータとして保持する
					currentField.push_back(currentChar);

					if (currentChar == '\n')
					{
						currentLine++;
					}
					else if (currentChar == '\r' && (i + 1 >= _text.size() || _text[i + 1] != '\n'))
					{
						currentLine++;
					}
				}
				break;

			case CSVFieldState::QuoteClosed:
				if (currentChar == ',')
				{
					finishField();
				}
				else if (currentChar == '\r' || currentChar == '\n')
				{
					if (currentChar == '\r' && i + 1 < _text.size() && _text[i + 1] == '\n')
					{
						i++;
					}

					finishRecord();
					currentLine++;
				}
				else
				{
					// 閉じる引用符の後にはカンマ・改行・EOFだけを許可する
					_outErrorLine = currentLine;
					return false;
				}
				break;
			}
		}
		// 引用符を閉じずにファイル末尾へ到達した
		if (state == CSVFieldState::Quoted)
		{
			_outErrorLine = currentLine;
			return false;
		}

		// ファイル末尾に改行がなくても、最後の行を登録する
		if (recordTouched || !currentRow.empty() || !currentField.empty() || state == CSVFieldState::QuoteClosed)
		{
			finishRecord();
		}

		// 全処理に成功してから出力へ渡す
		_outRecords = std::move(records);
		_outErrorLine = 0;
		return true;
	}

	// BCryptOpenAlgorithmProviderで取得したアルゴリズムプロバイダーを自動解放する
	struct BCryptAlgorithmGuard
	{
		BCryptAlgorithmGuard() = default;

		// 二重解放防止のコピーガード
		BCryptAlgorithmGuard(const BCryptAlgorithmGuard&) = delete;
		BCryptAlgorithmGuard& operator=(const BCryptAlgorithmGuard&) = delete;

		~BCryptAlgorithmGuard()
		{
			if (handle) BCryptCloseAlgorithmProvider(handle, 0);
		}
		BCRYPT_ALG_HANDLE handle{ nullptr };
	};

	// BCryptGEnerateSymmetricKeyで取得した対象キーハンドルを自動解放できるようにする
	struct BCryptKeyGuard
	{
		BCryptKeyGuard() = default;

		// 二重解放防止のコピーガード
		BCryptKeyGuard(const BCryptKeyGuard&) = delete;
		BCryptKeyGuard& operator=(const BCryptKeyGuard&) = delete;
		~BCryptKeyGuard()
		{
			if (handle) BCryptDestroyKey(handle);
		}

		BCRYPT_KEY_HANDLE handle{ nullptr };
	};

}

bool File::ReadAllText(const char* _filePath, std::string& _outText)
{
	std::vector<std::uint8_t> bytes{};
	// ファイルをバイト列として読む
	if (!ReadAllBytes(_filePath, bytes)) return false;

	std::size_t textBegin{ 0 };
	// UTF-8BOM(EF BB BF)がついている場合は読み飛ばす
	if (bytes.size() >= 3 && bytes[0] == 0xEF && bytes[1] == 0xBB && bytes[2] == 0xBF)
	{
		textBegin = 3;
	}
	std::string temporaryText{};
	// 空ファイルまたはBOMしかないファイルも対応できるようにする
	if (textBegin < bytes.size())
	{
		temporaryText.assign(reinterpret_cast<const char*>(bytes.data() + textBegin), bytes.size() - textBegin);
	}
	
	// UTF-8として正しい文字列かを検査する(空の場合は正常なため検査しない)
	if (!temporaryText.empty() && TextEncoding::ToUtf16(temporaryText).empty())
	{
		DEBUG_LOG_ERROR("UTF-8ではない文字列が含まれています FilePath : {}\n", _filePath);
		return false;
	}

	// 全て成功してから出力へ移す
	_outText = std::move(temporaryText);
	return true;
}

bool File::WriteAllText(const char* _filePath, const std::string& _text)
{
	// ToUTF-16は文字数をintでWindowsAPIへ渡すのでintの上限を超える文字列は変換できない
	if (_text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
	{
		DEBUG_LOG_ERROR("書き込むテキストが大きすぎます FilePath : {}\n", _filePath ? _filePath : "(null)");
		return false;
	}

	// この関数はUTF-8テキスト保存用なので不正なUTF-8が渡された時には書き込まずに失敗させる(空は0byteテキスト)
	if (!_text.empty() && TextEncoding::ToUtf16(_text).empty())
	{
		DEBUG_LOG_ERROR("UTF-8ではない文字列が含まれています FilePath : {}\n", _filePath ? _filePath : "(null)");
		return false;
	}

	// stringが持っているUTF-8の各バイトをWriteAllBytesへ渡せるuint_8t配列へコピー
	const std::vector<std::uint8_t> bytes(_text.begin(), _text.end());
	// フォルダ作成などはWriteALlBytesの方へ任せる
	return WriteAllBytes(_filePath, bytes);
}

bool File::ReadAllBytes(const char* _filePath, std::vector<std::uint8_t>& _outBytes)
{
	if (!_filePath || _filePath[0] == '\0')
	{
		DEBUG_LOG_ERROR("ReadAllBytesに空のファイルパスが渡されました\n");
		return false;
	}

	// 公開APIで受け取ったUTF8のパスをWindows用のUTF-16へ変換
	const std::wstring widePath{ TextEncoding::ToUtf16(_filePath) };
	if (widePath.empty())
	{
		DEBUG_LOG_ERROR("ファイルパスのUTF-16変換に失敗しました FilePath : {}\n", _filePath);
		return false;
	}

	// ateを指定して開いたときの読み込み位置をファイル末尾にしてtellgでサイズを取る
	std::ifstream file{ std::filesystem::path{widePath}, std::ios::binary | std::ios::ate };
	if (!file.is_open())
	{
		DEBUG_LOG_ERROR("ファイルを開けませんでした FilePath : {}", _filePath);
		return false;
	}

	const std::streampos endPosition{ file.tellg() };
	// tellg失敗時は負の値が返る
	if (endPosition < std::streampos{ 0 })
	{
		DEBUG_LOG_ERROR("ファイルサイズを取得できませんでした FilePath : {}\n", _filePath);
		return false;
	}

	// ファイルのサイズ
	const auto fileSize{ static_cast<std::uintmax_t>(endPosition) };
	// readが受け取れる最大値を超えていないか確認
	if (fileSize > static_cast<std::uintmax_t>(std::numeric_limits<std::streamsize>::max()))
	{
		DEBUG_LOG_ERROR("ファイルサイズが大きすぎます FilePath {}\n", _filePath);
		return false;
	}
	
	// 途中失敗したときにoutBytesを壊さないように一時領域へ読み込む
	std::vector<std::uint8_t> tempiraryBytes(static_cast<std::size_t>(fileSize));
	// 読み込み位置を末尾から先頭へseekする
	file.seekg(0, std::ios::beg);
	if (!tempiraryBytes.empty())
	{
		// 読み込み
		file.read(reinterpret_cast<char*>(tempiraryBytes.data()), static_cast<std::streamsize>(tempiraryBytes.size()));
		if (!file)
		{
			DEBUG_LOG_ERROR("ファイルの読み込みに失敗しました\n");
			return false;
		}
	}

	_outBytes = std::move(tempiraryBytes);
	return true;
}

bool File::WriteAllBytes(const char* _filePath, const std::vector<std::uint8_t>& _bytes)
{
	// nullptrや空文字列をパスとして使用しない
	if (!_filePath || _filePath[0] == '\0')
	{
		DEBUG_LOG_ERROR("WriteAllBytesに空のファイルが渡されました\n");
		return false;
	}

	// ofstream::writeへ渡せる最大サイズを超えていないか確認
	if (_bytes.size() > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
	{
		DEBUG_LOG_ERROR("書き込みデータが大きすぎます FilePath : {}\n", _filePath);
		return false;
	}

	// UTF-8パスをWindows用のUTF-16へ変換
	const std::wstring widePath{ TextEncoding::ToUtf16(_filePath) };
	if (widePath.empty())
	{
		DEBUG_LOG_ERROR("ファイルパスのUTF-16変換に失敗しました FilePath : {}\n", _filePath);
		return false;
	}

	const std::filesystem::path destinationPath{ widePath };
	// 保存先フォルダが存在しなければ作る
	const std::filesystem::path parentPath{ destinationPath.parent_path() };
	if (!parentPath.empty())
	{
		std::error_code directoryError{};
		std::filesystem::create_directories(parentPath, directoryError);
		if (directoryError)
		{
			DEBUG_LOG_ERROR("保存先フォルダを作成できませんでした\n");
			return false;
		}
	}

	// 本番ファイルと同じフォルダに一時ファイルを作る(アトミック保存)
	std::filesystem::path temporaryPath{ destinationPath };
	temporaryPath += L".tmp";
	{
		// truncにより既存の一時ファイルがあれば内容を空にする
		std::ofstream file{ temporaryPath, std::ios::binary | std::ios::trunc };
		if (!file.is_open())
		{
			DEBUG_LOG_ERROR("一時ファイルを開けませんでした FilePath : {} \n", _filePath);
			return false;
		}
		// 空データの場合も0バイトファイルとして正常に保存する
		if (!_bytes.empty())
		{
			file.write(reinterpret_cast<const char*>(_bytes.data()), static_cast<std::streamsize>(_bytes.size()));

			if (!file)
			{
				DEBUG_LOG_ERROR("ファイルの書込みに失敗しました FilePath : {}\n", _filePath);
				file.close();
				std::error_code removeError{};
				std::filesystem::remove(temporaryPath, removeError);
				return false;
			}
		}
		// C++側の出力バッファをOSへ渡す
		file.flush();
		if (!file)
		{
			DEBUG_LOG_ERROR("ファイルのFlushに失敗しました FilePath : {}\n", _filePath);
			file.close();

			std::error_code removeError{};
			std::filesystem::remove(temporaryPath, removeError);
			return false;
		}
	}

	// 一時ファイルを本番ファイルへ置換する REPLACE_EXISTINGで既存を上書きしてWRITE_THROUGHで移動処理完了待ち
	const BOOL moveResult{ MoveFileExW(temporaryPath.c_str(), destinationPath.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) };

	if (!moveResult)
	{
		const DWORD errorCode{ GetLastError() };

		DEBUG_LOG_ERROR("一時ファイルの置換に失敗しました FilePath : {} ErrorCode : {}\n", _filePath, errorCode);
		std::error_code removeError{};
		std::filesystem::remove(temporaryPath, removeError);
		return false;
	}
	return true;
}

bool File::LoadCSV(const char* _filePath, CSVTable& _outTable)
{
	// CSVファイルをUTF-8テキストとして読む
	std::string text{};
	if (!ReadAllText(_filePath, text)) return false; // ReadAllText側でログを出しているのでここはそのまま

	// パーサーのヘルパーを使って文字列を行と列へ分解
	std::vector<std::vector<std::string>> records{};
	std::size_t errorLine{ 0 };
	if (!ParseCSVText(text, records, errorLine))
	{
		DEBUG_LOG_ERROR("CSVの構文が不正です FilePath : {} Line : {}\n", _filePath, errorLine);
		return false;
	}

	// このライブラリでは先頭行をヘッダーとして扱うので有効な行が1行もなければCSVとして読み込まない
	if (records.empty())
	{
		DEBUG_LOG_ERROR("CSVにヘッダーがありません FilePath : {}\n", _filePath);
		return false;
	}

	// 途中で失敗してもoutTabelが変更されないように一時領域を使う
	CSVTable temporaryTable{};
	// 先頭レコードをヘッダーとして移動する
	temporaryTable.headers = std::move(records.front());

	// ヘッダー名検索
	for (std::size_t i = 0; i < temporaryTable.headers.size(); i++)
	{
		if (temporaryTable.headers[i].empty())
		{
			DEBUG_LOG_ERROR("CSVのヘッダー名が空です FilePath : {} Column : {}\n", _filePath, i);
			return false;
		}

		// 同じ列名が複数あるとFindColumnで特定できないため区別する
		for (std::size_t j = 0; j < i; j++)
		{
			if (temporaryTable.headers[i] == temporaryTable.headers[j])
			{
				DEBUG_LOG_ERROR("CSVに重複したヘッダーがあります FilePath : {} Header : {}\n", _filePath, temporaryTable.headers[i]);
				return false;
			}
		}
	}

	// 2レコード目以降をデータ行として登録する
	for (std::size_t recordIndex = 1; recordIndex < records.size(); recordIndex++)
	{
		// 各行の列数はヘッダーの列数と一致する必要がある
		if (records[recordIndex].size() != temporaryTable.headers.size())
		{
			DEBUG_LOG_ERROR("CSVの列数がヘッダーと一致しません FilePath : {} Record : {} Expected : {} Actual : {}\n", _filePath, recordIndex + 1, temporaryTable.headers.size(), records[recordIndex].size());
			return false;
		}
		temporaryTable.rows.push_back(std::move(records[recordIndex]));
	}

	// 読み込みとすべての検査に成功してから入れる
	_outTable = std::move(temporaryTable);
	return true;
}

bool File::WriteEncryptedBytes(const char* _filePath, const std::vector<std::uint8_t>& _plainBytes, const CryptoKey& _key)
{
	// 暗号処理を始める前にパスを検査する
	if (!_filePath || _filePath[0] == '\0')
	{
		DEBUG_LOG_ERROR("WriteEncryptedBytesに空のファイルパスが渡されました\n");
		return false;
	}
	// BCryptEncryptが受け取るサイズはULONGなので上限を検査する
	if (_plainBytes.size() > static_cast<std::size_t>(std::numeric_limits<ULONG>::max()))
	{
		DEBUG_LOG_ERROR("暗号化するデータが大きすぎます FilePath : {}\n", _filePath);
		return false;
	}
	// CryptoKey{}のまま使うのを防ぐ
	bool hasNonZeroKeyByte{ false };
	for (const std::uint8_t keyByte : _key.bytes)
	{
		if (keyByte != 0)
		{
			hasNonZeroKeyByte = true;
			break;
		}
	}
	if (!hasNonZeroKeyByte)
	{
		DEBUG_LOG_ERROR("暗号鍵がすべて0です FilePath : {}\n", _filePath);
		return false;
	}

	// 変数の寿命に合わせて解放(RAII)なので以降returnしても自動解放される
	BCryptAlgorithmGuard algorithm{};
	BCryptKeyGuard key{};

	// AESアルゴリズムプロバイダーを開く
	NTSTATUS status{ BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) };
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("AESプロバイダーを開けませんでした status = 0x{:08X}\n", static_cast<std::uint32_t>(status));
		return false;
	}
	// AESの動作モードをGCMへと変更する
	status = BCryptSetProperty(algorithm.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)), 0);
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("AESのモードをGCMへ変更できませんでした status = 0x{:08X}\n", static_cast<std::uint32_t>(status));
		return false;
	}
	// 公開APIで受け取った32バイトのKeyからBCryptEcryptで使用するキーハンドルを作る
	status = BCryptGenerateSymmetricKey(algorithm.handle, &key.handle, nullptr, 0, reinterpret_cast<PUCHAR>(const_cast<std::uint8_t*>(_key.bytes.data())), static_cast<ULONG>(_key.bytes.size()), 0);
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("AES-256キーハンドルを作成できませんでした status = 0x{:08X}\n", static_cast<std::uint32_t>(status));
		return false;
	}
	// 同じ鍵でも毎回異なる暗号文になるようにNonceを生成
	std::array<std::uint8_t, GCM_NONCE_SIZE> nonce{};
	status = BCryptGenRandom(nullptr, reinterpret_cast<PUCHAR>(nonce.data()), static_cast<ULONG>(nonce.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("Nonceの乱数生成に失敗しました status = 0x{:08X}\n", static_cast<std::uint32_t>(status));
		return false;
	}
	// 暗号ファイルの認証対象となるヘッダー
	std::array<std::uint8_t, ENCRYPTED_HEADER_SIZE> header{};
	std::copy(ENCRYPTED_FILE_MAGIC.begin(), ENCRYPTED_FILE_MAGIC.end(), header.begin()); // 先頭4byteへTSGFを保存
	header[4] = ENCRYPTED_FILE_VERSION;
	header[5] = ENCRYPTION_AES_256_GCM;
	// header[6],[7]は将来拡張用
	// 暗号文サイズを8byteのリトルエンディアンで保存
	const std::uint64_t cipherSize{ static_cast<std::uint64_t>(_plainBytes.size()) };
	for (std::size_t i = 0; i < sizeof(cipherSize); i++)
	{
		// 狙ったバイトを最下位に落とす
		header[8 + i] = static_cast<std::uint8_t>((cipherSize >> (i * 8)) & 0xFF);
	}
	// GCMでは暗号文と平文のサイズが同じ
	std::vector<std::uint8_t> cipherBytes(_plainBytes.size());
	std::array<std::uint8_t, GCM_TAG_SIZE> tag{};

	// AES-GCMへNonce,AAD,Tagの保存先を伝える
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo{};
	BCRYPT_INIT_AUTH_MODE_INFO(authInfo);

	authInfo.pbNonce = reinterpret_cast<PUCHAR>(nonce.data());
	authInfo.cbNonce = static_cast<ULONG>(nonce.size());
	// ヘッダーは暗号化しないが改ざん検知の対象に
	authInfo.pbAuthData = reinterpret_cast<PUCHAR>(header.data());
	authInfo.cbAuthData = static_cast<ULONG>(header.size());
	// BCryptEncryptがここへTagを書く
	authInfo.pbTag = reinterpret_cast<PUCHAR>(tag.data());
	authInfo.cbTag = static_cast<ULONG>(tag.size());
	ULONG encryptedSize{ 0 };
	// 空データでも有効なポインタを渡せるようにダミーを用意する
	std::uint8_t dummyInput{ 0 };
	std::uint8_t dummyOutput{ 0 };
	PUCHAR plainData{ _plainBytes.empty() ? &dummyInput : reinterpret_cast<PUCHAR>(const_cast<std::uint8_t*>(_plainBytes.data())) };
	PUCHAR cipherData{ cipherBytes.empty() ? &dummyOutput : reinterpret_cast<PUCHAR>(cipherBytes.data()) };

	// 平文をAES-256-GCMで暗号化する
	status = BCryptEncrypt(key.handle, plainData, static_cast<ULONG>(_plainBytes.size()), &authInfo, nullptr, 0, cipherData, static_cast<ULONG>(cipherBytes.size()), &encryptedSize, 0);
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("AES-256-GCMの暗号化に失敗しましたStatus=0x{:08X}\n", static_cast<std::uint32_t>(status));
		return false;
	}
	if (encryptedSize != cipherBytes.size())
	{
		DEBUG_LOG_ERROR("暗号文サイズが想定と一致しません Expected={} Actual={}\n", cipherBytes.size(), encryptedSize);
		return false;
	}
	// ファイル全体を入れる領域がオーバーフローしないか検査する
	if (cipherBytes.size() > std::numeric_limits<std::size_t>::max() - ENCRYPTED_PAYLOAD_OFFSET)
	{
		DEBUG_LOG_ERROR("暗号ファイル全体のサイズが大きすぎます\n");
		return false;
	}
	// Header + Nonce + Tag + CipherTextを1つのバイト列へまとめる
	std::vector<std::uint8_t> encryptedFile(ENCRYPTED_PAYLOAD_OFFSET + cipherBytes.size());

	std::copy(header.begin(), header.end(), encryptedFile.begin());
	std::copy(nonce.begin(), nonce.end(), encryptedFile.begin() + ENCRYPTED_HEADER_SIZE);
	std::copy(tag.begin(), tag.end(), encryptedFile.begin() + ENCRYPTED_HEADER_SIZE + GCM_NONCE_SIZE);
	std::copy(cipherBytes.begin(), cipherBytes.end(), encryptedFile.begin() + ENCRYPTED_PAYLOAD_OFFSET);
	// 実際のファイル保存とアトミック置換は既存関数へ任せる
	return WriteAllBytes(_filePath, encryptedFile);
}

bool File::ReadEncryptedBytes(const char* _filePath, const CryptoKey& _key, std::vector<std::uint8_t>& _outPlainBytes)
{
	// 暗号ファイル全体をバイト列として読む
	std::vector<std::uint8_t> encryptedFile{};

	if (!ReadAllBytes(_filePath, encryptedFile))
	{
		return false;
	}

	// Header + Nonce + Tagが最低限必要
	if (encryptedFile.size() < ENCRYPTED_PAYLOAD_OFFSET)
	{
		DEBUG_LOG_ERROR("暗号ファイルのサイズが小さすぎます FilePath : {}\n", _filePath);
		return false;
	}

	// 先頭4バイトがTSGFか検査する
	if (!std::equal(ENCRYPTED_FILE_MAGIC.begin(), ENCRYPTED_FILE_MAGIC.end(), encryptedFile.begin()))
	{
		DEBUG_LOG_ERROR("TSGameLibの暗号ファイルではありません FilePath : {}\n", _filePath);
		return false;
	}
	// 読み込み側が対応しているファイル形式か検査する
	if (encryptedFile[4] != ENCRYPTED_FILE_VERSION)
	{
		DEBUG_LOG_ERROR("対応していない暗号ファイルバージョンです FilePath : {} Version : {}\n", _filePath, encryptedFile[4]);
		return false;
	}
	// AES-256-GCM形式か検査する
	if (encryptedFile[5] != ENCRYPTION_AES_256_GCM)
	{
		DEBUG_LOG_ERROR("対応していない暗号方式です FilePath : {} Algorithm : {}\n", _filePath, encryptedFile[5]);
		return false;
	}
	// Version 1では予約領域は0でなければならない
	if (encryptedFile[6] != 0 || encryptedFile[7] != 0)
	{
		DEBUG_LOG_ERROR("暗号ファイルの予約領域が不正です FilePath : {}\n", _filePath);
		return false;
	}
	// ヘッダーに保存されている8バイトのサイズを復元する
	std::uint64_t storedCipherSize{ 0 };
	for (std::size_t i = 0; i < sizeof(storedCipherSize); ++i)
	{
		storedCipherSize |= static_cast<std::uint64_t>(encryptedFile[8 + i]) << (i * 8);
	}
	// 実際にファイル内へ存在する暗号文サイズ
	const std::size_t actualCipherSize{ encryptedFile.size() - ENCRYPTED_PAYLOAD_OFFSET };
	// ヘッダー値と実ファイルサイズが一致するか検査する
	if (storedCipherSize != static_cast<std::uint64_t>(actualCipherSize))
	{
		DEBUG_LOG_ERROR("暗号文サイズがヘッダーと一致しません FilePath : {} Header : {} Actual : {}\n", _filePath, storedCipherSize, actualCipherSize);
		return false;
	}
	// BCryptDecryptが受け取れる最大サイズを確認する
	if (storedCipherSize > static_cast<std::uint64_t>(std::numeric_limits<ULONG>::max()))
	{
		DEBUG_LOG_ERROR("暗号文が大きすぎます FilePath : {}\n", _filePath);
		return false;
	}

	// すべて0の鍵は拒否する
	const bool hasNonZeroKeyByte{std::any_of(_key.bytes.begin(), _key.bytes.end(),
			[](uint8_t _value)
			{
				return _value != 0;
			})
	};
	if (!hasNonZeroKeyByte)
	{
		DEBUG_LOG_ERROR("暗号鍵が設定されていません\n");
		return false;
	}
	// AESアルゴリズムプロバイダーと鍵を作成する
	BCryptAlgorithmGuard algorithm{};
	BCryptKeyGuard key{};

	// プロバイダーを開く
	NTSTATUS status{ BCryptOpenAlgorithmProvider(&algorithm.handle, BCRYPT_AES_ALGORITHM, nullptr, 0) };
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("AESアルゴリズムプロバイダーの作成に失敗しました Status : {}\n", static_cast<unsigned long>(status));
		return false;
	}

	// AESの暗号化方式をGCMに設定する
	status = BCryptSetProperty(algorithm.handle, BCRYPT_CHAINING_MODE, reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)), sizeof(BCRYPT_CHAIN_MODE_GCM), 0);
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("AES-GCMの設定に失敗しました Status : {}\n", static_cast<unsigned long>(status));
		return false;
	}
	// ユーザーから渡された鍵をCNGの鍵オブジェクトに変換する
	status = BCryptGenerateSymmetricKey(algorithm.handle, &key.handle, nullptr, 0, reinterpret_cast<PUCHAR>( const_cast<uint8_t*>(_key.bytes.data())), static_cast<ULONG>(_key.bytes.size()), 0);
	if (!BCRYPT_SUCCESS(status))
	{
		DEBUG_LOG_ERROR("復号鍵の作成に失敗しました Status : {}\n", static_cast<unsigned long>(status));
		return false;
	}

	// ファイル内の各領域を指すポインタを作る
	PUCHAR headerData{ reinterpret_cast<PUCHAR>(encryptedFile.data()) };
	PUCHAR nonceData{ reinterpret_cast<PUCHAR>(encryptedFile.data() + ENCRYPTED_HEADER_SIZE) };
	PUCHAR tagData{ reinterpret_cast<PUCHAR>(encryptedFile.data() + ENCRYPTED_HEADER_SIZE + GCM_NONCE_SIZE) };
	PUCHAR cipherData{ reinterpret_cast<PUCHAR>(encryptedFile.data() + ENCRYPTED_PAYLOAD_OFFSET) };

	// 認証付き暗号に渡す情報を設定する
	BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo{};
	BCRYPT_INIT_AUTH_MODE_INFO(authInfo);

	authInfo.pbNonce = nonceData;
	authInfo.cbNonce = static_cast<ULONG>(GCM_NONCE_SIZE);

	// ヘッダーも認証対象にする
	// 暗号化時と同じ範囲を指定しなければ認証に失敗する
	authInfo.pbAuthData = headerData;
	authInfo.cbAuthData = static_cast<ULONG>(ENCRYPTED_HEADER_SIZE);

	// 復号時には、保存されていた認証タグを検証用に渡す
	authInfo.pbTag = tagData;
	authInfo.cbTag = static_cast<ULONG>(GCM_TAG_SIZE);

	// 復号が成功するまで出力引数には書き込まない
	std::vector<uint8_t> temporaryPlainBytes(actualCipherSize);

	uint8_t dummyCipherByte{};
	uint8_t dummyPlainByte{};

	// 空データの場合もAPIへ安全なポインタを渡す
	PUCHAR decryptInput{ actualCipherSize == 0 ? &dummyCipherByte : cipherData };
	PUCHAR decryptOutput{ temporaryPlainBytes.empty() ? &dummyPlainByte : reinterpret_cast<PUCHAR>(temporaryPlainBytes.data()) };
	ULONG decryptedSize{ 0 };

	status = BCryptDecrypt(key.handle, decryptInput, static_cast<ULONG>(actualCipherSize), &authInfo, nullptr, 0, decryptOutput, static_cast<ULONG>(temporaryPlainBytes.size()), &decryptedSize, 0);
	if (!BCRYPT_SUCCESS(status))
	{
		// 鍵が違う、またはヘッダー・nonce・tag・暗号文の
		// いずれかが改ざんされている場合もここへ来る
		if (!temporaryPlainBytes.empty()) SecureZeroMemory(temporaryPlainBytes.data(), temporaryPlainBytes.size());
		DEBUG_LOG_ERROR("暗号化ファイルの復号または認証に失敗しました FilePath : {} Status : {}\n", _filePath, static_cast<unsigned long>(status));
		return false;
	}

	// APIが報告した復号サイズも確認する
	if (decryptedSize != temporaryPlainBytes.size())
	{
		if (!temporaryPlainBytes.empty()) SecureZeroMemory(temporaryPlainBytes.data(), temporaryPlainBytes.size());
		DEBUG_LOG_ERROR("復号後のデータサイズが一致しません FilePath : {} Expected : {} Actual : {}\n", _filePath, temporaryPlainBytes.size(), decryptedSize);
		return false;
	}

	// 認証まで成功したデータだけを呼び出し側へ渡す
	_outPlainBytes = std::move(temporaryPlainBytes);
	return true;
}
