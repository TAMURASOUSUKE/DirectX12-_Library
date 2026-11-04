// SetMaterialTextureのSlot0〜3が番号どおりに届くか確認する
#include "../../Src/Shaders/SpriteContract.hlsli"

// どのスロットを表示するか
cbuffer SlotSelectParameter : register(b0, space1)
{
	int slotIndex;
	float3 padding;
}

// SetMaterialTextureのSlot0〜3
Texture2D userTex0 : register(t0, space1);
Texture2D userTex1 : register(t1, space1);
Texture2D userTex2 : register(t2, space1);
Texture2D userTex3 : register(t3, space1);

float4 main(SpriteVertexOutput _input) : SV_TARGET
{
	// 4枚全て読んでから選ぶ
	const float4 c0 = userTex0.Sample(spriteSampler, _input.uv);
	const float4 c1 = userTex1.Sample(spriteSampler, _input.uv);
	const float4 c2 = userTex2.Sample(spriteSampler, _input.uv);
	const float4 c3 = userTex3.Sample(spriteSampler, _input.uv);
	
	float4 color = c0;
	if (slotIndex == 1)
	{
		color = c1;
	}
	else if (slotIndex == 2)
	{
		color = c2;
	}
	else if (slotIndex == 3)
	{
		color = c3;
	}
	
	// nullSRVが渡されている場合は全ての値が0で返るのでalphaを1にして黒にする
	return float4(color.rgb, 1.0f) * float4(_input.color.rgb, 1.0f);
}
