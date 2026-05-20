#pragma once
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d12.lib")

// 画面に表示させるためのデバッグとして三角形を管理する用
class DebugTriangle
{
public:
	void Initialize(ID3D12Device* _device);
	void Draw(ID3D12GraphicsCommandList* _commandList);

private:
	// 頂点定義
	struct Vertex
	{
		float position[3];
		float color[4];
	};

private:
	ComPtr<ID3D12Resource> vertexBuffer; // 頂点バッファを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{}; // 頂点バッファのビューを作る
};