#pragma pack_matrix(row_major)
#include "../Src/Shaders/ModelContract.hlsli"

// VFX02（描画状態）のテスト用PS
// 照明なしの単色を返す。rgbは色、aは強さ（Blendなら不透明度、Additiveなら足す量）
cbuffer VfxBlendTestParameterCB : register(b0, space1)
{
	float4 tintColor;
};

float4 main(PS_INPUT _input) : SV_Target
{
	return tintColor;
}
