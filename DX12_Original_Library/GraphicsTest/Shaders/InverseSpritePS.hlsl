// 色を反転させる
#include "../../Src/Shaders/SpriteContract.hlsli"

// SpriteのRGBを反転させる
float4 main(SpriteVertexOutput _input) : SV_TARGET
{
    float4 color = spriteTexture.Sample(spriteSampler, _input.uv);
    // αは透明度なので変更しない
    return float4(1.0f - color.rgb, color.a) * _input.color;
    
}