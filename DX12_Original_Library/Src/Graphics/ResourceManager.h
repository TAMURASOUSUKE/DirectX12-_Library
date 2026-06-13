#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <stack>
#include "../Core/Handle/TexHandle.h"
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;
#pragma comment(lib, "d3d12.lib")

// ShaderSystemやファサードがリソース管理を意識せず使えるようにするクラス
class ResourceManager
{
public:
	// シングルトン化
	static ResourceManager& Instance()
	{
		static ResourceManager instance;
		return instance;
	}


	// 初期化処理
	void Initialize(ID3D12Device* _device);

	// 頂点バッファの作成(Map->UnMapの固定)
	VertexBuffer CreateVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize);

	// 頂点バッファの作成(Mapしっぱなしで動的に確保を行う)
	VertexBuffer CreateDynamicVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize);

	// バッファを作るのはなく、確保とMapのみする関数
	DynamicBuffer CreateDynamicBuffer(UINT _dataSize);

	// インデックスバッファの作成
	IndexBuffer CreateIndexBuffer(const void* _data, UINT _dataSize, UINT _indexCount);

	// 定数バッファの作成
	ConstantBufferData CreateConstantBuffer(const void* _data, UINT _dataSize);

	// ファイル名を引数に画像をロードする関数
	TexHandle LoadTexture(const char* _filePath);

private:
	// コンストラクタ
	ResourceManager() = default;

	// コピー禁止
	ResourceManager(const ResourceManager& _other) = delete;
	ResourceManager& operator =(const ResourceManager& _other) = delete;
	
private:
	ID3D12Device* device{ nullptr }; // Initializeでデバイスを受け取って保持する
	std::vector<TextureSlot> texSlot; // テクスチャリソースのスロット
	std::stack<int> texFreeList; // テクスチャリソースのフリーリスト
};