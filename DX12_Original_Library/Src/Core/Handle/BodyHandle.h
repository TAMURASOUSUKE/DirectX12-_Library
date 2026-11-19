#pragma once
#include "HandlePassKey.h"

// 物理が動かす一つ分の記録を持つハンドル
class BodyHandle
{
public:
	BodyHandle() = default;
	BodyHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部システムだけが生値を取得する
	int GetRaw(PassKey _key) const { return value; }

	bool operator==(BodyHandle _other) const { return value == _other.value; }
	bool operator!=(BodyHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};
