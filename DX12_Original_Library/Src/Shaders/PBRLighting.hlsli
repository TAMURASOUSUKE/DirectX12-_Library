#ifndef TS_PBR_LIGHTING_HLSLI
#define TS_PBR_LIGHTING_HLSLI

// PBR計算で共通して使用する方向と内積
struct PBRGeometry
{
	float3 N; // ワールド空間の面法線
	float3 L; // 表面からライトへ向かう方向
	float3 V; // 表面からカメラへ向かう方向
	float3 H; // LとVの中間方向
	float NdotL; // 面がライトを向いている割合
	float NdotV; // 面がカメラを向いている割合
	float NdotH; // 微細面が中間方向を向いている割合
	float VdotH; // Fresnel計算で使う視線と中間方向の内積
};

// ゼロベクトルをnormalizeしてNanになるのを防ぐ
float3 SafeNormalize(float3 _value, float3 _fallback)
{
	const float lengthSquared = dot(_value, _value);
	// rsqrtは1 / sqrtを求める
	if (lengthSquared > 1.0e-6f)
	{
		return _value * rsqrt(lengthSquared);
	}
	return _fallback;
}

// PBRで共通する方向と内積を作る
PBRGeometry MakePBRGeometry(float3 _worldNormal, float3 _worldPosition, float3 _cameraPosition, float3 _lightRayDirection)
{
	PBRGeometry geometry;
	// ワールド法線
	geometry.N = SafeNormalize(_worldNormal, float3(0.0f, 1.0f, 0.0f)); // ゼロベクトルなら真上
	geometry.L = SafeNormalize(-_lightRayDirection, geometry.N); // PBRでは表面からライト方向なので反転
	geometry.V = SafeNormalize(_cameraPosition - _worldPosition, geometry.N); // 表面からカメラへ向かう方向
	geometry.H = SafeNormalize(geometry.L + geometry.V, geometry.N); // ライト方向と視線方向の中間
	
	// 0-1に制御することで内積結果で得られる背面の-1を外してライティングへ使用しない
	geometry.NdotL = saturate(dot(geometry.N, geometry.L));
	geometry.NdotV = saturate(dot(geometry.N, geometry.V));
	geometry.NdotH = saturate(dot(geometry.N, geometry.H));
	geometry.VdotH = saturate(dot(geometry.V, geometry.H));
	return geometry;
}


#endif
