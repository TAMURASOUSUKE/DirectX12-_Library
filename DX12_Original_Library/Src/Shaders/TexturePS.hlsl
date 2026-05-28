// テクスチャを表示するためのピクセルシェーダー
Texture2D tex : register(t0); // テクスチャスロット0番
SamplerState smp : register(s0); // サンプラースロット0番

struct PS_INPUT
{
    float4 position : SV_POSITION; // 座標
    float2 uv : TEXCOORD; // uv座標 
};

// EntryPoint
float4 main(PS_INPUT _input) : SV_TARGET
{
    // 色をそのまま出す
    return tex.Sample(smp, _input.uv);

}