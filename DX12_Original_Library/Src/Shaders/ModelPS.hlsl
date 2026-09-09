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
	uint alphaMode; // αモード
	float alphaCutoff; // ピクセル切り捨ての基準
	float pad3;
	float pad4;
};

Texture2D baseTex : register(t0);
Texture2D metallicRoughnessTex : register(t1);
SamplerState smp : register(s0);
Texture2D<float> shadowMap : register(t2);
SamplerComparisonState shadowSampler : register(s1);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
	float3 worldNormal : NORMAL0;
	float3 worldPosition : POSITION0;
	float4 shadowPosition : TEXCOORD1;
};

// 影を判定する関数
float CalculateShadowVisibility(float4 _shadowPosition)
{
	// クリップ座標をNDCへ変換
	const float3 shadowNDC = _shadowPosition.xyz / _shadowPosition.w;
	// NDCのＸ・Yは-1~1、TextureのUVは0-1 Texture座標はYが反転しているので考慮する
	const float2 shadowUV = { shadowNDC.x * 0.5f + 0.5f, -shadowNDC.y * 0.5f + 0.5f };
	
	    // ShadowMapの描画範囲外は影にしない
	if (shadowUV.x < 0.0f || shadowUV.x > 1.0f ||  shadowUV.y < 0.0f || shadowUV.y > 1.0f || shadowNDC.z < 0.0f || shadowNDC.z > 1.0f)
	{
		return 1.0f;
	}
	
	uint shadowWidth;
	uint shadowHeight;
	shadowMap.GetDimensions(shadowWidth, shadowHeight);
	// ShadowMap上の1ピクセル分のUVサイズ
	const float2 texelSize = 1.0f / float2(shadowWidth, shadowHeight);
	 // 同じ面が自分自身を影と誤判定するのを軽減する
	const float shadowBias = 0.0005f;
	const float comparisonDepth = shadowNDC.z - shadowBias;
	float visibility = 0.0f;
	
	// PCFを使って周囲の深度の比較結果を平均して影の境界をぼかす
	[unroll] // 繰り返し処理を展開する
	for (int y = -1; y <= 1; y++)
	{
		[unroll]
		for (int x = -1; x <= 1; x++)
		{
			const float2 offset = float2(x, y) * texelSize;
			visibility += shadowMap.SampleCmpLevelZero(shadowSampler, shadowUV + offset, comparisonDepth);
		}
	}
	return visibility / 9.0f;
}

float4 main(PS_INPUT _input) : SV_Target
{
	// sRGBテクスチャはSRVで線形色へ変換された状態で取得される
	const float4 baseColor = baseTex.Sample(smp, _input.uv) * baseColorFactor;
	
	// Mask判定
	static const uint materialAlphaModeMask = 1;
	if (alphaMode == materialAlphaModeMask)
	{
		clip(baseColor.a - alphaCutoff); // 指定値で引き算を行い0未満になれば破棄する
	}
	
	const float4 metallicRoughnessSample = metallicRoughnessTex.Sample(smp, _input.uv);
	 
	// glTFではG = Rougness, B = Metallicが格納されている
	const float materialRoughness = roughness * metallicRoughnessSample.g;
	const float materialMetallic = metallic * metallicRoughnessSample.b;
	
	const PBRGeometry geometry = MakePBRGeometry(_input.worldNormal, _input.worldPosition, cameraPosition.xyz, directionalDirectionAndIntensity.xyz);
	const float3 directLight = CalculateCookTorranceDirectLight(geometry, baseColor.rgb, materialMetallic, materialRoughness, directionalColor.rgb, directionalDirectionAndIntensity.w);

	// どのくらい影がかかっているか
	const float shadowVisibility = CalculateShadowVisibility(_input.shadowPosition);
	
	// IBL実装前の暫定的な環境光(本来のPBRであれば環境光も拡散IBLと鏡面IBLに分ける)
	const float3 ambientLight = baseColor.rgb * ambientColorAndIntensity.rgb * ambientColorAndIntensity.w;
	const float3 finalColor = directLight * shadowVisibility + ambientLight;
	return float4(finalColor, baseColor.a);
}
