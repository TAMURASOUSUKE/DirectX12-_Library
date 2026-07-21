// テクスチャを表示するための基本的なシェーダー
#include "SpriteContract.hlsli"


// EntryPoint
// 頂点座標を画面座標へ変換する
SpriteVertexOutput main(SpriteVertexInput _input)
{
    SpriteVertexOutput output; // 返す用の構造体
    output.position = mul(float4(_input.position, 1.0f), orthogonalProjectionMat);
    output.uv = _input.uv; // そのまま渡す
    output.color = _input.color;
    return output;
}