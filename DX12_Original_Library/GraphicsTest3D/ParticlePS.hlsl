#pragma pack_matrix(row_major)
#include "../Src/Shaders/VFXContract.hlsli"
#include "ParticleLook.hlsli"

// 粒子のPS：柔らかい光（glow）と細い芯（core）を重ね、尾側（u=0）を暗くする
// テクスチャを使わず、UVから距離で形を作る。星などの形にしたければここを差し替える
float4 main(VFXVertexOutput _input) : SV_Target
{
	// [数学] UV(0〜1)を中心原点(-1〜1)にして中心からの距離の2乗を取る（板が伸びていれば楕円になる）
	const float2 centered = _input.uv * 2.0f - 1.0f;
	const float falloff = saturate(1.0f - dot(centered, centered)); // 中心で1、縁で0

	const float glow = falloff * falloff;   // 広い光
	const float core = pow(falloff, 8.0f);  // 細い芯（加算なら1を超えて白く飛ぶ）
	const float tail = lerp(tailFade, 1.0f, _input.uv.x); // 尾ほど暗く（tailFade = 1 なら均一）

	const float intensity = (glow * glowStrength + core * coreStrength) * tail;

	// additive = 1：色にも強さを掛ける（加算合成用）。0：色はそのまま、不透明度だけ形に従う（アルファ合成用）
	const float3 rgb = _input.color.rgb * lerp(1.0f, intensity, additive);
	return float4(rgb, _input.color.a * intensity);
}
