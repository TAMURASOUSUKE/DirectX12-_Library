// テクスチャを表示するための基本的なシェーダー
#include "Lighting.hlsli"
#include "PBRLighting.hlsli"
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう

cbuffer SceneFrameCB : register(b0)
{
	float4x4 viewProjection;
	float4 cameraPosition;
}

// material用定数バッファ
cbuffer MaterialCB : register(b1)
{
    float4 baseColorFactor; // 基本色
    float metallic; // 金属色
    float roughness; // 粗さ
    float pad0; // パディング
    float pad1;  // パディング
    float3 emissiveColorFactor; // 自己発光色
    float pad2; // パディング
};

Texture2D tex : register(t0);
SamplerState smp : register(s0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
	float3 worldNormal : NORMAL0;
	float3 worldPosition : POSITION0;
};

float4 main(PS_INPUT _input) : SV_Target
{
	//const float4 baseColor = tex.Sample(smp, _input.uv) * baseColorFactor; // 基本的な色
	//const float3 sceneLight = CalculateSceneLight(_input.worldNormal); // ライティング
	
	//// 通常色はライト乗算、emissiveはemissiveTexture対応時に実装
	//const float3 finalColor = baseColor.rgb * sceneLight;
	//return float4(finalColor, baseColor.a);
	
	// 一時デバッグ
	const PBRGeometry geometry = MakePBRGeometry(_input.worldNormal, _input.worldPosition, cameraPosition.xyz, directionalDirectionAndIntensity.xyz);
	const float emphasized = pow(1.0f - geometry.VdotH, 5.0f);
	return float4(emphasized.xxx,1.0f);
	// NdotLを白黒で表示する
	return float4(geometry.NdotH.xxx, 1.0f);

}
