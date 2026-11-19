#ifndef TS_LIGHTING_HLSLI
#define TS_LIGHTING_HLSLI

// C++側SceneLightCBと配置一致させる
cbuffer SceneLightCB : register(b3)
{
	// xyz：光線が進む方向 w：平行光源の強さ
	float4 directionalDirectionAndIntensity;
	// xyz：平行光源の色 w：未使用
	float4 directionalColor;
	// xyz：環境光の色 w：環境光の強さ
	float4 ambientColorAndIntensity;
}

// 法線から平行光源と環境光の合計を求める
float3 CalculateSceneLight(float3 _worldNormal)
{
	const float3 normal = normalize(_worldNormal);

	// C++側は光線が進む方向を表しているため面から光源に向かう場合は符号反転
	const float3 toLight = normalize(-directionalDirectionAndIntensity.xyz);
	// 面が光源を向いている割合
	const float diffuseFactor = saturate(dot(normal, toLight));
	const float3 directionalResult = directionalColor.rgb * directionalDirectionAndIntensity.w * diffuseFactor;
	const float3 ambientResult = ambientColorAndIntensity.rgb * ambientColorAndIntensity.w;

	return directionalResult + ambientResult;
}

#endif
