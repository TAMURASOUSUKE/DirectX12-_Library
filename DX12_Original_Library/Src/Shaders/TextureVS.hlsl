// テクスチャを表示するための基本的なシェーダー
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう


// 定数バッファ
cbuffer ConstantBuffer : register(b0)
{
   float4x4 orthogonalProjectionMat; // 正射影行列
}

struct VS_INPUT
{
    float3 position : POSITION; // 座標
    float2 uv : TEXCOORD; // UV座標
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION; // 座標
    float2 uv : TEXCOORD; // UV座標
};

// EntryPoint
VS_OUTPUT main(VS_INPUT _input)
{
    VS_OUTPUT output; // 返す用の構造体
    output.position = mul(float4(_input.position, 1.0f), orthogonalProjectionMat);
    output.uv = _input.uv; // そのまま渡す
    return output;
}