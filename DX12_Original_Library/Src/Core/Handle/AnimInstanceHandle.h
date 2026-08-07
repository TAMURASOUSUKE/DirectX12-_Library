#pragma once
#include "HandlePassKey.h"

// 3Dアニメーション個体を識別するハンドル
class AnimInstanceHandle
{
public:
	AnimInstanceHandle() = default;
	AnimInstanceHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部システムだけが生値を取得する
	int GetRaw(PassKey _key) const { return value; }

	bool operator==(AnimInstanceHandle _other) const { return value == _other.value; }
	bool operator!=(AnimInstanceHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};
