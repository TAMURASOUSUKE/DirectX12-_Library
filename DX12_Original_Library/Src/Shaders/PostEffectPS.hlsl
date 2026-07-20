#include "PostEffectContract.hlsli" // 共通項目

float4 main(PostEffectVertexOutput _input) : SV_TARGET
{
    // 今は加工せずにそのまま出力する
   return sceneTexture.Sample(sceneSampler, _input.uv);

    //// グレースケール
    //float4 sceneColor = sceneTexture.Sample(sceneSampler, _input.uv);
    //// RGBから明るさを計算する(シーンRTのSRVがSRGBなのでsample結果はリニア空間になっている)
    //float gray = dot(sceneColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    
    //// アルファ値を維持する
    //return float4(gray, gray, gray, sceneColor.a);
    
}