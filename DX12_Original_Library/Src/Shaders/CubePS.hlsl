// テクスチャを表示するためのピクセルシェーダー

struct PS_INPUT
{
    float4 position : SV_POSITION; // 座標
    float4 color : COLOR; // 色 
};

// EntryPoint
float4 main(PS_INPUT _input) : SV_TARGET
{
    // 色をそのまま出す
    return _input.color;

}