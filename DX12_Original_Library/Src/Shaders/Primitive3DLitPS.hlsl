#include "Primitive3DContract.hlsli"
#include "Lighting.hlsli"

// 面の色にシーンライトを反映する
float4 main(Primitive3DVSOutput _input) : SV_TARGET
{
	// 平行光源と環境光を合算した光量
	const float3 sceneLight = CalculateSceneLight(_input.normal);
	
	// RGBに光を掛けてAは変更しない
	return float4(_input.color.rgb * sceneLight, _input.color.a);
}
