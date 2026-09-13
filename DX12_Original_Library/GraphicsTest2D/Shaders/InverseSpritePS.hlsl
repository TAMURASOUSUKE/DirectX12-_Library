// 色を反転させる
#include "../../Src/Shaders/SpriteContract.hlsli"

// MaterialParameterSlot0
cbuffer InverseParameter : register(b0, space1)
{
    float strength;
    float3 padding;
}

// SpriteのRGBを反転させる
float4 main(SpriteVertexOutput _input) : SV_TARGET
{
    float4 textureColor = spriteTexture.Sample(spriteSampler, _input.uv);
    
    const float3 inverseColor = 1.0f - textureColor.rgb;
    
    // 0なら通常1なら完全反転
    const float3 resultColor = lerp(textureColor.rgb, inverseColor, saturate(strength));
    
    return float4(resultColor, textureColor.a) * _input.color;
    
}
