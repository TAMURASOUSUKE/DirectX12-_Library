#include "CSVTable.h"

int CSVTable::FindColumn(const char* _columnName) const
{
	// nullptrやから文字列はれつめいとして扱えない
	if (!_columnName || _columnName[0] == '\0') return -1;
	
	// ヘッダーを先頭から探す
	for (std::size_t i = 0; i < headers.size(); i++)
	{
		// 列名は大文字小文字を区別して完全一致で対応
		if (headers[i] == _columnName)
		{
			return static_cast<int>(i);
		}
	}
	return -1;
}
