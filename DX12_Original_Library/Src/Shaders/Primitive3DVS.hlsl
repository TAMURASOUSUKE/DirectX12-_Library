#include "Primitive3DContract.hlsli"

Primitive3DVSOutput main(Primitive3DVSInput _input, uint _instanceID : SV_InstanceID)
{
	Primitive3DVSOutput output;

	// DrawIndexedInstancedが割り当てた個体番号から専用のworld行列と色を取得
	Primitive3DInstanceData instance = instanceData[_instanceID];
	
	// 単位メッシュの頂点をワールド空間へ配置する
	float4 worldPosition = mul(float4(_input.position, 1.0f), instance.world);
	// 画面座標へ変換
	output.position = mul(worldPosition, viewProjection);
	output.worldPosition = worldPosition.xyz;
	
	// w = 0で位置移動を法線には適用しない
	output.normal = normalize(mul(float4(_input.normal, 0.0f), instance.worldInverseTranspose).xyz);
	output.color = instance.color;
	return output;
}
