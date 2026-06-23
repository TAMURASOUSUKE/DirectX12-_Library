// テクスチャを表示するための基本的なシェーダー
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう

cbuffer MVP : register(b0) // ルートパラメータ[0]のCBV
{
    float4x4 mvp;
}

struct VS_INPUT
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
};

struct VS_OUTPUT
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD;
};

VS_OUTPUT main(VS_INPUT _input)
{
    VS_OUTPUT output;
    output.position = mul(float4(_input.position, 1.0f), mvp); // mvpを掛ける
    output.uv = _input.uv;
    return output;
}