// テクスチャを表示するためのピクセルシェーダー
#include "SpriteContract.hlsli"

// EntryPoint
// テクスチャの色をそのまま出力するシェーダー
float4 main(SpriteVertexOutput _input) : SV_TARGET
{
    // 色をそのまま出す
    return spriteTexture.Sample(spriteSampler, _input.uv);
}