#pragma once
#include <string>
#include <vector>
#include <cstdint>
#include <array>
#include "../File/CSVTable.h"

// 汎用的な外部ファイル操作機能を提供する(正常に読み込めた空のファイルを区別するために戻り値はbool)
namespace File
{
	// AES-256で使用する256bit = 32byteのカギ
	struct CryptoKey
	{
		std::array<std::uint8_t, 32> bytes{};
	};

	// UTF-8テキストファイルをすべて読み込む
	bool ReadAllText(const char* _filePath, std::string& _outText);
	// UTF-8テキストファイルを上書き保存する
	bool WriteAllText(const char* _filePath, const std::string& _text);
	// バイナリファイルをすべて読み込む
	bool ReadAllBytes(const char* _filePath, std::vector<std::uint8_t>& _outBytes);
	// バイト列を上書きする
	bool WriteAllBytes(const char* _filePath, const std::vector<std::uint8_t>& _bytes);
	// CSVをヘッダーと行データに分解して読み込む
	bool LoadCSV(const char* _filePath, CSVTable& _outTable);
	// 平文を暗号化してファイルへ保存する
	bool WriteEncryptedBytes(const char* _filePath, const std::vector<std::uint8_t>& _plainBytes, const CryptoKey& _key);
	// 暗号化ファイルを読み込み認証後に復号する
	bool ReadEncryptedBytes(const char* _filePath, const CryptoKey& _key, std::vector<std::uint8_t>& _outPlainBytes);
}
