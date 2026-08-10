#pragma pack_matrix(row_major)

// 全基礎図形で共有するカメラ情報
cbuffer Primitive3DFrameCB : register(b0)
{
	float4x4 viewProjection;
}

// C++側のPrimitiveInstanceDataと同じ順・型
struct Primitive3DInstanceData
{
	float4x4 world;
	float4x4 worldInverseTranspose;
	float4 color;
};

// 個体情報を並べた配列
StructuredBuffer<Primitive3DInstanceData> instanceData : register(t0);

struct Primitive3DVSInput
{
	float3 position : POSITION;
	float3 normal : NORMAL;
};

struct Primitive3DVSOutput
{
	float4 position : SV_POSITION;
	float3 worldPosition : POSITION0;
	float3 normal : NORMAL0;
	float4 color : COLOR0;
};
