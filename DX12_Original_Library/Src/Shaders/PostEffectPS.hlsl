
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
    return sceneTexture.Sample(sceneSampler, _input.uv);

}