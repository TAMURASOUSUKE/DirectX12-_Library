#include "../../Src/Shaders/SpriteContract.hlsli"

// slot0 = b0
// 時間とエフェクトの強さ
cbuffer GlitchParameter : register(b0, space1)
{
    float time;
    float strength;
    float chromaticOffset;
    float scanlineCount;
}

// slot1 = b1
// グリッチ発生部分へ加える色
cbuffer GlitchColorParameter : register(b1, space1)
{
    float4 accentColor;
}

// slot = b2
// アトラス対応用
cbuffer AtlasParameter : register(b2, space1)
{
	// xy = 現在のコマの左上UV
	// zw = 現在のコマの右下UV
	float4 atlasUVRect;
	float bandCount;
	float3 padding;
}

// 0-1の疑似乱数
float Hash(float _value)
{
    return frac(sin(_value) * 43785.5453f);
}

// コマ内の0-1の座標をアトラス全体のUVへ戻す
float2 ConvertToAtlasUV(float2 _localUV, float2 _rectMin, float2 _rectSize, float2 _halfTexel)
{
	// 現在のコマから外へ出ないようにする
	_localUV = saturate(_localUV);
	
	// コマ内UVからアトラス全体のUVへ
	float2 atlasUV = _rectMin + _localUV * _rectSize;
	
	// Linerサンプリングが隣のコマを混ぜないようにコマ境界から半テクセルだけ内側へ制限
	const float2 sampleMin = _rectMin + _halfTexel;
	const float2 sampleMax = _rectMin + _rectSize - _halfTexel;
	return clamp(atlasUV, sampleMin, sampleMax);
}

float4 main(SpriteVertexOutput _input) : SV_TARGET
{
	float2 rectMin = atlasUVRect.xy;
	float2 rectMax = atlasUVRect.zw;
	float2 rectSize = rectMax - rectMin;
	
	 // AtlasParameterが未設定の場合、ゼロダミーCBが渡される。
    // その場合は画像全体を使用して、0除算を防ぐ。
	if (rectSize.x <= 0.0f || rectSize.y <= 0.0f)
	{
		rectMin = float2(0.0f, 0.0f);
		rectSize = float2(1.0f, 1.0f);
	}
	
	// アトラス全体のUVを現在のコマ内の0-1に変換する
	float2 localUV = (_input.uv - rectMin) / rectSize;
	
	 // テクスチャの1ピクセルがUV上でどれだけかを求める
	uint textureWidth;
	uint textureHeight;
	spriteTexture.GetDimensions(textureWidth, textureHeight);
	float2 halfTexel = 0.5f / float2(textureWidth, textureHeight);
	
	// 現在のコマ内のyで帯を作る
	float band = floor(localUV.y * bandCount);
	 // 時間を段階化し、グリッジ位置を一定間隔で切り替える
	float timeStep = floor(time * 12.0f);
	float noise = Hash(band + timeStep * 31.0f);
   
    // 一部の横帯だけグリッチを発生させる
    float glitchGate = step(0.82f, noise) * saturate(strength);
    // 帯ごとに左右どちらへずらすか決める
    float direction = Hash(band * 7.0f + timeStep) * 2.0f - 1.0f;
    float horizontalShift = direction * glitchGate * 0.06f;
    // 常時発生する小さな波を追加
	horizontalShift += sin(localUV.y * 60.0f + time * 10.0f) * 0.002f * saturate(strength);
    
    // SpriteのSamplerはWrapなのでUVが0-1を超えて反対側を拾わないように明示制限する
    float2 shiftedLocalUV = localUV + float2(horizontalShift, 0.0f);
	float2 shiftedAtlasUV = ConvertToAtlasUV(shiftedLocalUV, rectMin, rectSize, halfTexel);
	float4 centerSample = spriteTexture.Sample(spriteSampler, shiftedAtlasUV);
    
    // 完全透明な領域
    clip(centerSample.a - 0.001f);
    
    // RGBをそれぞれ少し違うUVから読み、色ずれを作る
    float chromatic = chromaticOffset * saturate(strength) * (0.25f + glitchGate);
	// 色ずれも各コマ基準で
	float2 redUV = ConvertToAtlasUV(shiftedLocalUV + float2(chromatic, 0.0f), rectMin, rectSize, halfTexel);
	float2 blueUV = ConvertToAtlasUV(shiftedLocalUV - float2(chromatic, 0.0f), rectMin, rectSize, halfTexel);
    float red = spriteTexture.Sample(spriteSampler, redUV).r;
    float green = centerSample.g;
    float blue = spriteTexture.Sample(spriteSampler, blueUV).b;
    
    float3 resultColor = float3(red, green, blue);
    
    // 横幅が大きくずれた場所へアクセント色を加える
    resultColor += accentColor.rgb * accentColor.a * glitchGate;
    // CRTのような細い走査線
    float scanline = 0.82f + 0.18f * sin((localUV.y * scanlineCount + time * 8.0f) * 6.283185f);
    
    // 高速でわずかに明滅させる
    float flicker = 0.94f + 0.06f * sin(time * 37.0f);
    // strengthが0なら走査線と明滅も無効
    resultColor *= lerp(1.0f, scanline * flicker, saturate(strength));
    return float4(saturate(resultColor), centerSample.a) * _input.color;
}
