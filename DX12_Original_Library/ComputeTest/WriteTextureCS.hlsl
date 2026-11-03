// textuerを作成して書き込む

// 画像情報
cbuffer TextureParamter : register(b0)
{
	uint width;
	uint height;
	uint2 padding;
}

// 出力
RWTexture2D<float> output : register(u0); 

[numthreads(8, 8, 1)] // x8, y8スレッドを実行
void main(uint3 id : SV_DispatchThreadID)
{
	// 幅を超えているスレッドを弾いて節約
	if (id.x >= width || id.y >= height)
	{
		return;
	}
	// indexを一列に
	output[id.xy] = id.x + id.y * width;
}
