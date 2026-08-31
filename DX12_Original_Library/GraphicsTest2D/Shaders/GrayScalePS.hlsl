// シーンをグレースケールにする
#include "../../Src/Shaders/PostEffectContract.hlsli"

// 自作構造体(slot0はb4)
cbuffer GrayScaleParameter : register(b4)
{
    float strength;
    float3 padding;
}

// Material Parameter Slot1はb5
cbuffer ColorOffsetParameter : register(b5)
{
    float3 colorOffset;
    float padding2;
};


// シーン全体をグレースケールにする
float4 main(PostEffectVertexOutput _input) : SV_TARGET
{
    float4 sceneColor = sceneTexture.Sample(sceneSampler, _input.uv);

    // sRGBのSRVから読み込んだ値はリニア空間なのでリニア空間用の係数で明るさを求める
    float gray = dot(sceneColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    
    // 0なら元画像,1なら完全なグレースケール
    sceneColor.rgb = lerp(sceneColor.rgb, float3(gray, gray, gray), saturate(strength));
    
    sceneColor.rgb = saturate(sceneColor.rgb + colorOffset);
    
    return sceneColor;

}
