// 三角形を描画する頂点シェーダー
// 受け取るデータ
struct VS_INPUT
{
    float3 position : POSITION; // 位置
    float4 color : COLOR; // 頂点カラー
};

// 次へと渡すデータ
struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

VS_OUTPUT main(VS_INPUT _input)
{
    // 情報をそのまま詰める
    VS_OUTPUT output;
    output.position = float4(_input.position, 1.0f); // 座標はそのまま返す
    output.color = _input.color;
    return output;
}