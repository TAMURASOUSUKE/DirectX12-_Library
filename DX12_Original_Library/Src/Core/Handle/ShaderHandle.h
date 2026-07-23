#pragma once
#include "HandleConstant.h"
// Shaderを外部から扱えるようにするためのハンドル型

class ShaderHandle
{
public:
	// デフォルトコンストラクタ(値は常に-1で無効)
	ShaderHandle() = default;
	// 作成にはKeyを要するコンストラクタ
	ShaderHandle(PassKey _key, int _pack) : value(_pack) {}

	// 内部の値を取得する(他が触れないようにここもKeyを要求する)
	int GetRaw(PassKey _key) const { return value; }

	// 比較
	bool operator == (ShaderHandle _other) const { return value == _other.value; }
	bool operator != (ShaderHandle _other) const { return value != _other.value; }

	// ハンドルが有効かどうかを調べる
	bool IsValid() const { return value >= 0; } // 先頭bitが未使用つまり有効なら非負になる

private:
	int value{ -1 }; // ハンドル本体(-1は未定義状態)
};