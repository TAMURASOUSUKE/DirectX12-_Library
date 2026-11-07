#pragma pack_matrix(row_major)
#include "../Src/Shaders/VFXContract.hlsli"

// VFX03（ワールド空間VFXメッシュ）のテスト用PS
// 照明なし。頂点色をそのまま返す（帯の端のフェードは頂点色のαで表す）
float4 main(VFXVertexOutput _input) : SV_Target
{
	return _input.color;
}
