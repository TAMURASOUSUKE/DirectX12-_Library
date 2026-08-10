#include "Primitive3DContract.hlsli"

float4 main(Primitive3DVSOutput _input) : SV_TARGET
{
	// 今はライトに対応せずに設定された色をそのまま表示する
	return _input.color;
}
