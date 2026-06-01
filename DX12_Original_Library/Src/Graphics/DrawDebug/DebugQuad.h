#pragma once
#pragma once
#include <d3d12.h>
#include "../GraphicsType.h"

#pragma comment(lib, "d3d12.lib")

// テクスチャを張り付けたQuadを描画するためのクラス
class DebugQuad
{
public:
	void Initialize(); // 初期化
	void Draw(ID3D12GraphicsCommandList* _cmdList); // 描画命令

private:
	// 頂点定義
	struct Vertex
	{
		float position[3]; // 座標
		float uv[2]; // uv座標 
	};

private:
	GPUBuffer vertexBuffer; // ReosurceManagerが返す構造体を保持する
	IndexBuffer indexBuffer; // インデックスバッファを保持する
	TextureData textureData; // 画像データ
};