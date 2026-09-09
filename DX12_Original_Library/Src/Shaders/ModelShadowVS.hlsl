// ShadowMapへモデルの進度を書き込むためのVertexShader
#pragma pack_matrix(row_major)

// ShadowPass全体で共通する光源ViewProjection
cbuffer ShadowFrameCB : register(b0)
{
	float4x4 lightViewProjection;
}

// モデル個体ごとの行列
cbuffer ModelObjectCB : register(b4)
{
	float4x4 world;
	float4x4 worldInverseTranspose; // Shadowでは使わないが通常モデル描画と同じObjectCBを共有するため残す
}

// アニメーションモデルのスキニング行列
cbuffer BoneCB : register(b2)
{
	float4x4 boneMatrices[256];
}

struct VS_INPUT
{
	float3 position : POSITION;
	float3 normal : NORMAL;
	float2 uv : TEXCOORD;
	float4 weight : WEIGHTS;
	uint4 bone : BONES;
};

float4 main(VS_INPUT _input) : SV_POSITION
{
	const float4 localPosition = float4(_input.position, 1.0f);
	
    // 頂点へ影響する最大4本のBone行列を重み付きで合成する
	const float4 skinnedPosition =
        mul(localPosition, boneMatrices[_input.bone.x]) * _input.weight.x +
        mul(localPosition, boneMatrices[_input.bone.y]) * _input.weight.y +
        mul(localPosition, boneMatrices[_input.bone.z]) * _input.weight.z +
        mul(localPosition, boneMatrices[_input.bone.w]) * _input.weight.w;
	
	 // Local → World → Light View → Light Projection
	const float4 worldPosition = mul(skinnedPosition, world);
	return mul(worldPosition, lightViewProjection);
}
