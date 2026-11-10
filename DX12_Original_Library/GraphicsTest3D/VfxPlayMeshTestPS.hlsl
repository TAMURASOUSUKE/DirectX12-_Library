#pragma pack_matrix(row_major)
#include "../Src/Shaders/VFXContract.hlsli"

// VFX05の残り（板と帯の再生管理）のテスト用PS
// 画像を使わず、計算で作ったノイズで溶ける様子を見る

// slot0 種類ごとの見た目（テンプレートのMaterialに設定した値がSlotごとの複製へ引き継がれる）
cbuffer VfxPlayMeshLook : register(b0, space1)
{
	float4 baseColor;  // 色
	float flowSpeed;   // 模様が横へ流れる速さ（uv/秒）
	float edgeWidth;   // 溶ける境目のぼかし幅
	float dissolve;    // 1で progress に合わせて溶ける 0で溶けない
	float lookPadding;
};

// slot3 ライブラリが再生中のMaterialへ毎フレーム書く時間（VFXTimeParameter と同じ並び）
cbuffer VFXTimeCB : register(b3, space1)
{
	float progress;    // 進み具合 0で開始 1で終了
	float elapsed;     // 再生開始からの秒数
	float duration;    // 再生する秒数
	float timePadding;
};

// 格子点ごとの乱数（0から1）
float Hash(float2 _p)
{
	return frac(sin(dot(_p, float2(127.1f, 311.7f))) * 43758.5453f);
}

// 4つの格子点の乱数をなめらかに混ぜたノイズ（0から1）
float ValueNoise(float2 _p)
{
	float2 cell = floor(_p);
	float2 f = frac(_p);
	f = f * f * (3.0f - 2.0f * f);
	float a = Hash(cell);
	float b = Hash(cell + float2(1.0f, 0.0f));
	float c = Hash(cell + float2(0.0f, 1.0f));
	float d = Hash(cell + float2(1.0f, 1.0f));
	return lerp(lerp(a, b, f.x), lerp(c, d, f.x), f.y);
}

float4 main(VFXVertexOutput _input) : SV_Target
{
	// 模様を時間で横へ流す
	float2 flowUV = _input.uv + float2(flowSpeed * elapsed, 0.0f);
	float noise = ValueNoise(flowUV * 8.0f);

	// 進み具合がノイズの値を超えた所から消える
	float remain = smoothstep(progress - edgeWidth, progress + edgeWidth, noise);
	remain = lerp(1.0f, remain, dissolve);

	// 上下の端を薄くする（vの中央が濃い）
	float mask = saturate(1.0f - abs(_input.uv.y * 2.0f - 1.0f));

	// 頂点色のαは帯の新しさ（板は常に1）
	return float4(baseColor.rgb, baseColor.a * mask * remain * _input.color.a);
}
