#pragma once
#include "HandlePassKey.h"

// VFXのメッシュ(板 帯)の種類を外部から識別するハンドル
class VFXMeshTypeHandle
{
public:
	VFXMeshTypeHandle() = default;
	VFXMeshTypeHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部システムだけが生値を取得する
	int GetRaw(PassKey _key) const { return value; }

	bool operator==(VFXMeshTypeHandle _other) const { return value == _other.value; }
	bool operator!=(VFXMeshTypeHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};
