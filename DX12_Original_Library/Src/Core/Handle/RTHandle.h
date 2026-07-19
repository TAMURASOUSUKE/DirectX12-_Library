#pragma once
#include "HandleConstant.h"

// RenderTargetを管理するハンドル
class RTHandle
{
public:
	RTHandle() = default;
	RTHandle(PassKey, int _packed) : value{ _packed } {}

	int GetRaw(PassKey) const { return value; }

	// 比較
	bool operator == (RTHandle _other) const { return value == _other.value; }
	bool operator != (RTHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};