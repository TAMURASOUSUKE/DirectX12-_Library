#include <filesystem>
#include <fstream>
#include <limits>
#include "../Core/TextEncoding.h"
#include "../Debug/DebugLogs.h"
#include "File.h"

bool File::ReadAllText(const char* _filePath, std::string& _outText)
{
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
