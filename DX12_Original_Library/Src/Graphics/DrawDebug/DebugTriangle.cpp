#include <cstring>
#include "DebugTriangle.h"

// 初期化
void DebugTriangle::Initialize(ID3D12Device* _device)
{
	if (_device == nullptr) return;

	// 頂点データの中身を埋める
	Vertex vertices[]
	{
		{{0.0f, 0.5f, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		{{-0.5f, -0.5f, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}},
	};

	const UINT vertexBufferSize{ sizeof(vertices) }; // バッファのサイズ定義

	// ヒープ
	D3D12_HEAP_PROPERTIES heapProps{};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;
    heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    heapProps.CreationNodeMask = 1;
    heapProps.VisibleNodeMask = 1;

    // リソース設定
    D3D12_RESOURCE_DESC resourceDesc{};
    resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    resourceDesc.Alignment = 0;
    resourceDesc.Width = vertexBufferSize;
    resourceDesc.Height = 1;
    resourceDesc.DepthOrArraySize = 1;
    resourceDesc.MipLevels = 1;
    resourceDesc.Format = DXGI_FORMAT_UNKNOWN;
    resourceDesc.SampleDesc.Count = 1;
    resourceDesc.SampleDesc.Quality = 0;
    resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    resourceDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

    // 頂点バッファ作成
    HRESULT result = _device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&vertexBuffer)
    );

    if (FAILED(result))
    {
        OutputDebugStringA("VertexBuffer creation failed.\n");
        return;
    }

    // GPUメモリに頂点データを書き込む
    void* mappedData = nullptr;

    result = vertexBuffer->Map(0, nullptr, &mappedData);

    if (FAILED(result))
    {
        OutputDebugStringA("VertexBuffer map failed.\n");
        return;
    }

    std::memcpy(mappedData, vertices, vertexBufferSize);

    vertexBuffer->Unmap(0, nullptr);

    // 頂点バッファビュー作成
    vertexBufferView.BufferLocation = vertexBuffer->GetGPUVirtualAddress();
    vertexBufferView.SizeInBytes = vertexBufferSize;
    vertexBufferView.StrideInBytes = sizeof(Vertex);
}

// 描画処理
void DebugTriangle::Draw(ID3D12GraphicsCommandList* _commandList)
{
    if (_commandList == nullptr) return;

    _commandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // リストでセットする
    _commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
    _commandList->DrawInstanced(3, 1, 0, 0);
}