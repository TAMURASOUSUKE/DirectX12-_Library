
// シーンRTを読むPixelShader
Texture2D sceneTexture : register(t0);
SamplerState sceneSampler : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
};

float4 main(PSInput _input) : SV_TARGET
{
    // 今は加工せずにそのまま出力する
   //  return sceneTexture.Sample(sceneSampler, _input.uv);

    // グレースケール
    float4 sceneColor = sceneTexture.Sample(sceneSampler, _input.uv);
    // RGBから明るさを計算する(シーンRTのSRVがSRGBなのでsample結果はリニア空間になっている)
    float gray = dot(sceneColor.rgb, float3(0.2126f, 0.7152f, 0.0722f));
    
    // アルファ値を維持する
    return float4(gray, gray, gray, sceneColor.a);
    
}