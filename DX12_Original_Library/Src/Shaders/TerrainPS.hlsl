// DSから受け取った色を出す
struct PSInput
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

float4 main(PSInput _input) : SV_TARGET
{
    return _input.color;
}