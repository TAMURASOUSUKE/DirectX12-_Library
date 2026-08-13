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
	// sRGBテクスチャはSRVで線形色へ変換された状態で取得される
	float debugMetallic = 1.0f;
	float debugRoughness = 0.8f;
	const float4 baseColor = tex.Sample(smp, _input.uv) * baseColorFactor;
	const PBRGeometry geometry = MakePBRGeometry(_input.worldNormal, _input.worldPosition, cameraPosition.xyz, directionalDirectionAndIntensity.xyz);
	const float3 directLight = CalculateCookTorranceDirectLight(geometry, baseColor.rgb, debugMetallic, debugRoughness, directionalColor.rgb, directionalDirectionAndIntensity.w);

	// IBL実装前の暫定的な環境光(本来のPBRであれば環境光も拡散IBLと鏡面IBLに分ける)
	const float3 ambientLight = baseColor.rgb * ambientColorAndIntensity.rgb * ambientColorAndIntensity.w;
	const float3 finalColor = directLight + ambientLight;
	return float4(finalColor, baseColor.a);

}
