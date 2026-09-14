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

#endif
