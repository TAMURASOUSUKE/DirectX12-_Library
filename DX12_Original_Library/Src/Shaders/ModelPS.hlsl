// テクスチャを表示するための基本的なシェーダー
#include "Lighting.hlsli"
#include "PBRLighting.hlsli"
#include "ModelContract.hlsli"
#pragma pack_matrix(row_major) // 全ての行列を行優先としてあつかう


float4 main(PS_INPUT _input) : SV_Target
{
	// sRGBテクスチャはSRVで線形色へ変換された状態で取得される
	const float4 baseColor = baseTex.Sample(smp, _input.uv) * baseColorFactor;
	
	// Mask判定
	static const uint materialAlphaModeMask = 1;
	if (alphaMode == materialAlphaModeMask)
	{
		clip(baseColor.a - alphaCutoff); // 指定値で引き算を行い0未満になれば破棄する
	}
	
	const float4 metallicRoughnessSample = metallicRoughnessTex.Sample(smp, _input.uv);
	 
	// glTFではG = Rougness, B = Metallicが格納されている
	const float materialRoughness = roughness * metallicRoughnessSample.g;
	const float materialMetallic = metallic * metallicRoughnessSample.b;
	
	const PBRGeometry geometry = MakePBRGeometry(_input.worldNormal, _input.worldPosition, cameraPosition.xyz, directionalDirectionAndIntensity.xyz);
	const float3 directLight = CalculateCookTorranceDirectLight(geometry, baseColor.rgb, materialMetallic, materialRoughness, directionalColor.rgb, directionalDirectionAndIntensity.w);

	// どのくらい影がかかっているか
	const float shadowVisibility = CalculateShadowVisibility(_input.shadowPosition);
	
	// IBL実装前の暫定的な環境光(本来のPBRであれば環境光も拡散IBLと鏡面IBLに分ける)
	const float3 ambientLight = baseColor.rgb * ambientColorAndIntensity.rgb * ambientColorAndIntensity.w;
	const float3 finalColor = directLight * shadowVisibility + ambientLight;
	return float4(finalColor, baseColor.a);
}
