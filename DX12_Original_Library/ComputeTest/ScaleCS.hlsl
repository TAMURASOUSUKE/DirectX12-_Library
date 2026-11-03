cbuffer ScaleParamter : register(b0)
{
	uint elementCount;
	uint addValue;
	uint2 padding;
}

StructuredBuffer<uint> input : register(t0); // 入力
RWStructuredBuffer<uint> output : register(u0); // 出力

[numthreads(64, 1, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= elementCount)
	{
		return;
	}
	
	output[id.x] = input[id.x] * 3;
}
