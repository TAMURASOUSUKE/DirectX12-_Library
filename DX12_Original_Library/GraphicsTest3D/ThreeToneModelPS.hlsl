#pragma pack_matrix(row_major)
#include "../Src/Shaders/ModelContract.hlsli"
#include "../Src/Shaders/Lighting.hlsli"

// 三段階Toonを実装する
cbuffer ThreeToneParameterCB : register(b0, space1)
{
	float4 shadowColor;
	float4 middleColor;
	float4 lightColor;

	float shadowThreshold;
	float lightThreshold;
	float shadowMapThreshold;
	float padding;
};

float4 main(PS_INPUT _input) : SV_Target
{
	const float4 baseColor = baseTex.Sample(smp, _input.uv) * baseColorFactor;
	static const uint MATERIAL_ALPHA_MODE_MASK = 1;
	if (alphaMode == MATERIAL_ALPHA_MODE_MASK)
	{
		clip(baseColor.a - alphaCutoff);
	}

	const float3 normal = normalize(_input.worldNormal);
	// C++側は光が進む方向なので反転
	const float3 toLight = normalize(-directionalDirectionAndIntensity.xyz);
	const float ndotl = saturate(dot(normal, toLight));
	
	// 最初は暗部色
	float3 selectedColor = shadowColor.rgb;
	// shadowThreshold以上なら中間色
	selectedColor = lerp(selectedColor, middleColor.rgb, step(shadowThreshold, ndotl));

	// lightThreshold以上なら明部色
	selectedColor = lerp(selectedColor, lightColor.rgb, step(lightThreshold, ndotl));

	// ShadowMap上で影なら、強制的に暗部色へ落とす
	const float shadowVisibility = CalculateShadowVisibility(_input.shadowPosition);

	selectedColor = lerp(shadowColor.rgb, selectedColor, step(shadowMapThreshold, shadowVisibility));

	const float3 finalColor = baseColor.rgb * selectedColor * directionalColor.rgb * directionalDirectionAndIntensity.w;

	return float4(finalColor, baseColor.a);
}
