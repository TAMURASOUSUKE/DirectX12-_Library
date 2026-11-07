#include "VFXContract.hlsli"

// VFXの標準的な頂点シェーダー

// UVと色はそのまま渡されたworld位置を行列変換して渡す
VFXVertexOutput main(VFXVertexInput _input)
{
	VFXVertexOutput output;
	output.position = float4(mul(float4(_input.position, 1.0f), viewProjection));
	output.uv = _input.uv;
	output.color = _input.color;
	return output;
}
