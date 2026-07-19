#pragma once
#include "HandleConstant.h"

// RenderTargetを管理するハンドル
class RTHandle
{
public:
	RTHandle() = default;
	RTHandle(PassKey, int _packed)
		: value{ _packed }
	{}

	int GetRaw(PassKey) const
	{
		return value;
	}

	bool IsValid() const
	{
		return value >= 0;
	}

private:
	int value{ -1 };
};