#pragma once
#include <d3d12.h>
#include "../GraphicsType.h"

#pragma comment(lib, "d3d12.lib")

// 画面に表示させるためのデバッグとして三角形を管理する用
class DebugTriangle
{
public:
	void Initialize();
	void Draw(ID3D12GraphicsCommandList* _commandList);

private:
	// 頂点定義
	struct Vertex
	{
		float position[3];
		float color[4];
	};

private:
	GPUBuffer vertexBuffer; // ReosurceManagerが返す構造体を保持する
};