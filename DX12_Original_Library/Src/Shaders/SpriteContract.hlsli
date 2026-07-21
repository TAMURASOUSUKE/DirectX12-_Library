#ifndef TS_SPRITE_CONTRACT_HLSLI
#define TS_SPRITE_CONTRACT_HLSLI

// Sprite関連の構造体や共通処理をまとめる

// ライブラリ側の行列規約に合わせる
#pragma pack_matrix(row_major)

// Spriteの頂点座標をNDCへ変換する正射影行列
// RootSignatureではVSだけから参照可能
cbuffer SpriteTransformCB : register(b0)
{
    float4x4 orthogonalProjectionMat;
};

// Spriteが表示するテクスチャ
// RootSignatureではPixelShaderだけから参照可能
Texture2D spriteTexture : register(t0);
// Spriteテクスチャを読み取るサンプラー
// RootSignatureではPixelShaderだけから参照可能
SamplerState spriteSampler : register(s0);

// Sprite頂点バッファからVSへ渡される入力
struct SpriteVertexInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

// VSからPSへ渡される出力
// 外部Sprite用PSもこの構造体を入力として受け取る
struct SpriteVertexOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

#endif