// テクスチャを表示するための基本的なシェーダー
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう


// 定数バッファ
cbuffer ConstantBuffer : register(b0)
{
    float4x4 mvpMat; // 透視投影行列
}

struct VS_INPUT
{
    float3 position : POSITION; // 座標
    float4 color : COLOR; // 色
};

struct VS_OUTPUT
{
    float4 position : SV_POSITION; // 座標
    float4 color : COLOR; // 色
};

// EntryPoint
VS_OUTPUT main(VS_INPUT _input)
{
    VS_OUTPUT output; // 返す用の構造体
    output.position = mul(float4(_input.position, 1.0f), mvpMat);
    output.color = _input.color; // そのまま渡す
    return output;
}