#pragma once
#include <string>
#include <vector>

// 汎用的なCSVファイルの読み込みを行った表データ
struct CSVTable
{
	std::vector<std::string> headers{}; // 先頭行にかかれた列名
	std::vector<std::vector<std::string>> rows{}; // 二行目以降のデータ

	// 指定した列名が何列目かを調べる(見つからないなら-1)
	int FindColumn(const char* _columnName) const;
};

