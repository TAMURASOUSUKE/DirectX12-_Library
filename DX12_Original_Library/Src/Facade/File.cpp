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
#include "../Core/TextEncoding.h"
#include "../Debug/DebugLogs.h"
#include "File.h"

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
	return true;
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
