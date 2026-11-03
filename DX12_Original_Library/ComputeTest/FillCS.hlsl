

// 要素の数と追加する値を持った定数バッファ
cbuffer FillParameter : register(b0)
{
	uint elementCount;
	uint addValue;
	uint2 padding;
};

// 出力 (読み書き可能なStructuredBuffer)
RWStructuredBuffer<uint> output : register(u0);

[numthreads(64, 1, 1)] // 1スレッドグループあたり64本のスレッドを持つ
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= elementCount)
	{
		return; // 要素数がグループの倍数でない時の余りを捨てる
	}
		output[id.x] = id.x * 2 + addValue;
	}
