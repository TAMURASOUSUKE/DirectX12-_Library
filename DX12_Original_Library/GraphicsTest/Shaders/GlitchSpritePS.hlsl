#include "../../Src/Shaders/SpriteContract.hlsli"

// slot0 = b4
// 時間とエフェクトの強さ
cbuffer GlitchParameter : register(b4)
{
    float time;
    float strength;
    float chromaticOffset;
    float scanlineCount;
}

// slot1 = b5
// グリッチ発生部分へ加える色
cbuffer GlitchColorParameter : register(b5)
{
    float4 accentColor;
}

// 0-1の疑似乱数
float Hash(float _value)
{
    return frac(sin(_value) * 43785.5453f);
}

float4 main(SpriteVertexOutput _input) : SV_TARGET
{
    float2 uv = _input.uv;
    // 横方向に画像を区切り、それぞれをグリッチ帯として扱う
    float band = floor(uv.y * 24.0f);
    // 時間を段階化し、グリッジ位置を一定間隔で切り替える
    float timeStep = floor(time * 12.0f);
    float noise = Hash(band + timeStep * 31.0f);
    
    // 一部の横帯だけグリッチを発生させる
    float glitchGate = step(0.82f, noise) * saturate(strength);
    // 帯ごとに左右どちらへずらすか決める
    float direction = Hash(band * 7.0f + timeStep) * 2.0f - 1.0f;
    float horizontalShift = direction * glitchGate * 0.06f;
    // 常時発生する小さな波を追加
    horizontalShift += sin(uv.y * 60.0f + time * 10.0f) * 0.002f * saturate(strength);
    
    // SpriteのSamplerはWrapなのでUVが0-1を超えて反対側を拾わないように明示制限する
    float2 shiftedUV = saturate(uv + float2(horizontalShift, 0.0f));
    float4 centerSample = spriteTexture.Sample(spriteSampler, shiftedUV);
    
    // 完全透明な領域
    clip(centerSample.a - 0.001f);
    
    // RGBをそれぞれ少し違うUVから読み、色ずれを作る
    float chromatic = chromaticOffset * saturate(strength) * (0.25f + glitchGate);
    float red = spriteTexture.Sample(spriteSampler, saturate(shiftedUV + float2(chromatic, 0.0f))).r;
    float green = centerSample.g;
    float blue = spriteTexture.Sample(spriteSampler, saturate(shiftedUV - float2(chromatic, 0.0f))).b;
    
    float3 resultColor = float3(red, green, blue);
    
    // 横幅が大きくずれた場所へアクセント色を加える
    resultColor += accentColor.rgb * accentColor.a * glitchGate;
    // CRTのような細い走査線
    float scanline = 0.82f + 0.18f * sin((uv.y * scanlineCount + time * 8.0f) * 6.283185f);
    
    // 高速でわずかに明滅させる
    float flicker = 0.94f + 0.06f * sin(time * 37.0f);
    // strengthが0なら走査線と明滅も無効
    resultColor *= lerp(1.0f, scanline * flicker, saturate(strength));
    return float4(saturate(resultColor), centerSample.a) * _input.color;
}