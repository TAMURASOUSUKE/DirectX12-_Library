#ifndef TS_MODEL_CONTRACT_HLSLI
#define TS_MODEL_CONTRACT_HLSLI

// モデル描画の際に使用される構造体等をまとめたヘッダ

// フレームで共通して使う
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
	float pad1; // パディング
	float3 emissiveColorFactor; // 自己発光色
	float pad2; // パディング
	uint alphaMode; // αモード
	float alphaCutoff; // ピクセル切り捨ての基準
	float pad3;
	float pad4;
};

// モデル個体ごと
cbuffer ModelObjectCB : register(b4)
{
	float4x4 world;
	float4x4 worldInverseTranspose;
}

cbuffer BoneCB : register(b2)
{
	float4x4 boneMatrices[256]; // スキニング行列
}

// ShadowPass全体で共通する光源ViewProjection
cbuffer ShadowFrameCB : register(b5)
{
	float4x4 lightViewProjection;
}

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

struct VS_INPUT
{
	float3 position : POSITION; // 位置
	float3 normal : NORMAL; // 法線
	float2 uv : TEXCOORD; // テクスチャ
	float4 weight : WEIGHTS; // 重み
	uint4 bone : BONES; // ボーン
};

// 影を判定する関数
float CalculateShadowVisibility(float4 _shadowPosition)
{
	// クリップ座標をNDCへ変換
	const float3 shadowNDC = _shadowPosition.xyz / _shadowPosition.w;
	// NDCのＸ・Yは-1~1、TextureのUVは0-1 Texture座標はYが反転しているので考慮する
	const float2 shadowUV = { shadowNDC.x * 0.5f + 0.5f, -shadowNDC.y * 0.5f + 0.5f };
	
	    // ShadowMapの描画範囲外は影にしない
	if (shadowUV.x < 0.0f || shadowUV.x > 1.0f || shadowUV.y < 0.0f || shadowUV.y > 1.0f || shadowNDC.z < 0.0f || shadowNDC.z > 1.0f)
	{
		return 1.0f;
	}
	
	uint shadowWidth = 0;
	uint shadowHeight = 0;
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

#endif
