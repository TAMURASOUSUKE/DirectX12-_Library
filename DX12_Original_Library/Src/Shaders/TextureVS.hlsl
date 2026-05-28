// テクスチャを表示するための基本的なシェーダー
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
    output.position = float4(_input.position, 1.0f); // そのまま渡す
    output.uv = _input.uv; // そのまま渡す
    return output;
}