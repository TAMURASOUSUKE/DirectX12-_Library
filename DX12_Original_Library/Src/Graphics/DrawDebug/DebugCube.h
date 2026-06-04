#pragma once
#include <d3d12.h>
#include "../GraphicsType.h"

#pragma comment(lib, "d3d12.lib")

class DebugCube
{
public:
	void Initialize(); // 初期化
	void Draw(ID3D12GraphicsCommandList* _commandList); // 描画命令

private:
	VertexBuffer vertexBuffer; // ReosurceManagerが返す構造体を保持する
	IndexBuffer indexBuffer; // インデックスバッファを保持する

};