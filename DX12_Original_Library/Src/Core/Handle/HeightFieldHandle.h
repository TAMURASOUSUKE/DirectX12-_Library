#pragma once
#include "HandlePassKey.h"

// 物理に登録した高さの格子を外部から識別するハンドル
class HeightFieldHandle
{
public:
	HeightFieldHandle() = default;
	HeightFieldHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部システムだけが生値を取得する
	int GetRaw(PassKey _key) const { return value; }

	bool operator==(HeightFieldHandle _other) const { return value == _other.value; }
	bool operator!=(HeightFieldHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};
