// 基礎図形を描画するためのシェーダー
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう

struct PS_INPUT
{
    float4 position : SV_POSITION; // 座標
    float4 color : COLOR; // カラー
};

// EntryPoint
float4 main(PS_INPUT _input) : SV_TARGET
{
    // 色をそのまま出す
    return _input.color;

}