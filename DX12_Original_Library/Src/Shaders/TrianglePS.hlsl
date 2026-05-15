// 三角形を表示するためのピクセルシェーダー

struct PS_INPUT
{
    // 受け取るデータ
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

// float4で色を出力するmain関数
float4 main(PS_INPUT _input) : SV_POSITION
{
    return _input.color;
}