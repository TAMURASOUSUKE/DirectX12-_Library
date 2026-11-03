// 書き出されたTextuerをよむ
cbuffer TextureParameter : register(b0)
{
	uint width;
	uint height;
	uint2 padding;
}

Texture2D<float> input : register(t0); // 入力されるTexture
RWStructuredBuffer<float> output : register(u0); // bufferとして出力する

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
		// 幅を超えているスレッドを弾いて節約
	if (id.x >= width || id.y >= height)
	{
		return;
	}
	output[id.x + id.y * width] = input.Load(int3(id.x, id.y, 0));
}
