#include "Primitive3DContract.hlsli"

// Grid・Axis・AABBなど、指定色を正確に表示するPS
float4 main(Primitive3DVSOutput _input) : SV_TARGET
{
	return _input.color;
}
