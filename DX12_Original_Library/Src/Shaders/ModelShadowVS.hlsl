// ShadowMapへモデルの進度を書き込むためのVertexShader
#pragma pack_matrix(row_major)
#include "ModelContract.hlsli"



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
