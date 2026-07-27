#pragma once
#include "HandlePassKey.h"

// サウンドをロードしたときに返すハンドルを作成する
class SoundHandle
{
public:
	// デフォルトコンストラクタ(デフォルトは-1で未定義状態)
	SoundHandle() = default;
	// キーを使って内部の値を設定するコンストラクタ
	SoundHandle(PassKey _key, int _value) : value{ _value } {}

	// 内部の値を取得する(他が触れないようにここもKeyを要求する)
	int GetRaw(PassKey _key) const { return value; }

	// 比較
	bool operator == (SoundHandle _other) const { return value == _other.value; }
	bool operator != (SoundHandle _other) const { return value != _other.value; }

	// ハンドルが有効かどうかを調べる
	bool IsValid() const { return value >= 0; } // 先頭bitが未使用つまり有効なら非負になる

private:
	int value{ -1 }; // 内部の値
};
