
// HSとDSで共有する構造体とCB
#pragma pack_matrix(row_major)

// VSからHS,HSからDSへ渡す制御点
// VSではまだSV_POSITIONへ変換しない
struct TerrainControlPoint
{
	float3 position : POSITION;
	float2 uv : TEXCOORD;
};

// HSがTessellatorへ渡す分割係数
struct TerrainPatchConstant
{
	// 三角形の3辺それぞれの分割係数
	float edge[3] : SV_TessFactor;
	// 三角形内部の分割係数
	float inside : SV_InsideTessFactor;
};

// テライン用定数バッファ
cbuffer TerrainCB : register(b0)
{
	float4x4 mvp;
	float4 color;
	float heightScale;
	float tessFactor;
	float2 padding;
}
