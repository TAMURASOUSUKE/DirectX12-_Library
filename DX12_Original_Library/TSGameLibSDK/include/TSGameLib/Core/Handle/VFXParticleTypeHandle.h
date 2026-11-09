#pragma once
#include "HandlePassKey.h"

// VFXの粒子の種類を外部から識別するハンドル
class VFXParticleTypeHandle
{
public:
	VFXParticleTypeHandle() = default;
	VFXParticleTypeHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部システムだけが生値を取得する
	int GetRaw(PassKey _key) const { return value; }

	bool operator==(VFXParticleTypeHandle _other) const { return value == _other.value; }
	bool operator!=(VFXParticleTypeHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};
