#pragma once
#include "HandlePassKey.h"

// 物理に登録した三角形の集まりを外部から識別するハンドル
class TriangleMeshHandle
{
public:
	TriangleMeshHandle() = default;
	TriangleMeshHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部システムだけが生値を取得する
	int GetRaw(PassKey _key) const { return value; }

	bool operator==(TriangleMeshHandle _other) const { return value == _other.value; }
	bool operator!=(TriangleMeshHandle _other) const { return value != _other.value; }

	bool IsValid() const { return value >= 0; }

private:
	int value{ -1 };
};
