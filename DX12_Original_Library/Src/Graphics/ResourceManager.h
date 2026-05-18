#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
using Microsoft::WRL::ComPtr;
#pragma comment(lib, "d3d12.lib")

// バッファ一つ分の情報をまとめた構造体
struct GPUBuffer
{
	ComPtr<ID3D12Resource> resource; // リソースオブジェクト
	D3D12_VERTEX_BUFFER_VIEW vertexView; // 頂点バッファビュー
	UINT sizeInBytes; // バッファ全体のサイズ
};

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

	// 頂点バッファの作成
	GPUBuffer CreateVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize);

private:
	// コンストラクタ
	ResourceManager();

	// コピー禁止
	ResourceManager(const ResourceManager& _other) = delete;
	ResourceManager& operator =(const ResourceManager& _other) = delete;
	
private:
	ID3D12Device* device{ nullptr }; // Initializeでデバイスを受け取って保持する

};