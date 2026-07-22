#include "PostEffectContract.hlsli" // 共通項目

float4 main(PostEffectVertexOutput _input) : SV_TARGET
{
   return sceneTexture.Sample(sceneSampler, _input.uv);
}