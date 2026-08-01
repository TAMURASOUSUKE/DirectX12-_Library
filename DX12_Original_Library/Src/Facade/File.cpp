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
			DEBUG_LOG_ERROR("CSVのヘッダー名が空です FilePath : {} Column\n", _filePath, i);
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
	return true;
}

bool File::ReadEncryptedBytes(const char* _filePath, const CryptoKey& _key, std::vector<std::uint8_t>& _outPlainBytes)
{
	return true;
}
