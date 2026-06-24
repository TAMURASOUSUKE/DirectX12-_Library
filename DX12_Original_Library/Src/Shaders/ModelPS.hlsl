// テクスチャを表示するための基本的なシェーダー
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう

Texture2D tex : register(t0);
SamplerState smp : register(s0);

struct PS_INPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

float4 main(PS_INPUT _input) : SV_Target
{
    // return float4(_input.uv, 0.0f, 1.0f); // UVを色として出す（テクスチャ不要）
    return tex.Sample(smp, _input.uv);
}