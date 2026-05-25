#include "DebugTriangle.h"

// 初期化
void DebugTriangle::Initialize()
{

	// 頂点データの中身を埋める
	Vertex vertices[]
	{
		{{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
	};

	vertexBuffer = ResourceManager::Instance().CreateVertexBuffer(vertices, sizeof(vertices), sizeof(Vertex)); // 頂点バッファの作成を行う
}

// 描画処理
void DebugTriangle::Draw(ID3D12GraphicsCommandList* _commandList)
{
    if (_commandList == nullptr) return;

    _commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // リストでセットする
    _commandList->IASetVertexBuffers(0, 1, &vertexBuffer.vertexView);
    _commandList->DrawInstanced(3, 1, 0, 0);
}