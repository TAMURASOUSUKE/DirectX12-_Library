#pragma once
#include <d3d12.h>
#include "../GraphicsType.h"

#pragma comment(lib, "d3d12.lib")

// 画面に表示させるためのデバッグとして三角形を管理する用
class DebugTriangle
{
public:
	void Initialize(); // 初期化
	void Draw(ID3D12GraphicsCommandList* _commandList); // 描画命令

private:
	VertexBuffer vertexBuffer; // ReosurceManagerが返す構造体を保持する
};