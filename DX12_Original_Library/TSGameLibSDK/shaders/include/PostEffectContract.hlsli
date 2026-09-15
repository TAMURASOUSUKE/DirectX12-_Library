#ifndef TS_POST_EFFECT_CONTRACT_HLSLI
#define TS_POST_EFFECT_CONTRACT_HLSLI

// register(b0)～register(b3)は、
// registerSpace1としてSetMaterialParameterのSlot0～3に予約されています。
// 外部PostEffectシェーダー側で必要なcbufferを宣言してください。

// ポストエフェクト対象となるシーン全体の画像
Texture2D sceneTexture : register(t0);

// シーン画像を読み取る固定サンプラー
SamplerState sceneSampler : register(s0);

// フルスクリーン三角形VSからPSへ渡す情報
struct PostEffectVertexOutput
{
	float4 position : SV_POSITION;
	float2 uv : TEXCOORD0;
};

#endif
