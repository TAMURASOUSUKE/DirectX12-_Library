#pragma once
#include "HandlePassKey.h"

// UIとしてのボタンをユーザー側に簡易的に扱えるようにするハンドル
class UIButtonHandle
{
public:
	UIButtonHandle() = default;
	UIButtonHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部システムだけが生値を取得する
	int GetRaw(PassKey _key) const { return value; }

	bool operator==(UIButtonHandle _other) const { return value == _other.value; }
	bool operator!=(UIButtonHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};
