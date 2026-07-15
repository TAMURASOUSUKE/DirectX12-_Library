// 生成座標を保管してY座標をHeightMapで変位させる
#include "Terrain.hlsli"

// RootParam[2]
Texture2D heightMap : register(t0);
// Terrain用StaticSamplerのs0
SamplerState heightSampler : register(s0);

// DSからPSへ渡すデータ
struct DSOutput
{
    float4 position : SV_POSITION;
    float4 color : COLOR0;
};

// 三角形領域
[domain("tri")]
DSOutput main(TerrainPatchConstant _patchConstant, float3 _barycentric : SV_DomainLocation, const OutputPatch<TerrainControlPoint, 3> _patch)
{
    DSOutput output;

    // SV_DomainLocationは三角形内部の重心座標
    // 3つの係数を足すと1になる
    float3 localPosition = _patch[0].position * _barycentric.x + _patch[1].position * _barycentric.y + _patch[2].position * _barycentric.z;
    
    // HeightMapを参照するUVも同じ比率で補間する
    float2 uv = _patch[0].uv * _barycentric.x + _patch[1].uv * _barycentric.y + _patch[2].uv * _barycentric.z;
    
    // DSではSampleではなくSampleLevelを使う(今回はミップレベルを0に明示して取る)
    float height = heightMap.SampleLevel(heightSampler, uv, 0).r;
    // HeightMap無しの場合はCPU側からheightScale = 0が渡されるため、白テクスチャを読んでも平坦になる
    localPosition.y += height * heightScale;
    
    // 変位を終えてからクリップ座標へ変換
    output.position = mul(float4(localPosition, 1.0f), mvp);
    // PSにはb0を公開していないのでDSから色を渡す
    output.color = color;
    return output;
}
