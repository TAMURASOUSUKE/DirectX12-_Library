// シーンをグレースケールにする
#include "../../Src/Shaders/PostEffectContract.hlsli"

// シーン全体をグレースケールにする
float4 main(PostEffectVertexOutput _input) : SV_TARGET
{
    float4 sceneColor = sceneTexture.Sample(sceneSampler, _input.uv);

    // sRGBのSRVから読み込んだ値はリニア空間なのでリニア空間用の係数で明るさを求める
    float gray = dot(sceneColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    return float4(gray, gray, gray, sceneColor.a);

}