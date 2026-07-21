// テクスチャを表示するためのピクセルシェーダー
#include "SpriteContract.hlsli"

// EntryPoint
// テクスチャの色をそのまま出力するシェーダー
float4 main(SpriteVertexOutput _input) : SV_TARGET
{
    float4 textureColor = spriteTexture.Sample(spriteSampler, _input.uv);
      // RGBはTint、Alphaは透明度として適用される
    return textureColor * _input.color;
}