#ifndef TS_VFX_CONTRACT_HLSLI
#define TS_VFX_CONTRACT_HLSLI
// VFX関連の構造体や共通処理をまとめる

// ライブラリ側の行列規約に合わせる
#pragma pack_matrix(row_major)

// register(b0)～register(b3)は、
// SetMaterialParameterのSlot0～3として予約されています。
// 外部VFXシェーダー側で必要なcbufferを宣言してください。

// ユーザーが作成またはロードしたテクスチャは、
// SetMaterialTextureのSlot0~3のSpace1として予約されています
// 外部Shader側で必要なテクスチャを定義してください

// 1フレームの間で使われるVP行列
cbuffer VFXFrameCB : register(b0)
{
	float4x4 viewProjection; // VP行列
};

// VFXが読み取る標準サンプラー
SamplerState spriteSampler : register(s0);

// VFX頂点バッファからVSへ渡される入力
struct VFXVertexInput
{
	float3 position : POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};

// VSからPSへ渡される出力
// 外部VFX用PSもこの構造体を入力として受け取る(自作したい場合は入力構造体の型とセマンティクスと同じ物を使ってください)
struct VFXVertexOutput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
	float4 color : COLOR0;
};

#endif
