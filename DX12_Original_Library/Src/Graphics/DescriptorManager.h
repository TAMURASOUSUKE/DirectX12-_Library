#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <stack>
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d12.lib")

// 用意するDiscriptorHeapのデータをまとめた構造体
struct HeapData
{
	ComPtr<ID3D12DescriptorHeap> heap; // ヒープ本体
	UINT descriptorSize{ 0 }; // View一つ分のサイズ
	std::stack<UINT> freeList; // 空きスロット番号のスタック
	UINT slotCount{ 0 }; // スロットの数
	D3D12_DESCRIPTOR_HEAP_TYPE type; // ヒープのタイプ
	D3D12_DESCRIPTOR_HEAP_FLAGS flags; // GPU可視かどうかを判断するフラグ
};

// ShaderVisibleのSRV,CBV,UAV、CPUOnlyのRTV、DSV用の3つのヒープを管理するクラス
class DescriptorManager
{
public:
	// シングルトン化
	static DescriptorManager& Instance();
	// デフォルトデストラクタ
	~DescriptorManager() = default;

	// 初期化
	void Setup(ID3D12Device* _device);
	// 終了処理
	void Shutdown();

	/// <summary>
	/// ヒープに割り当てる関数
	/// </summary>
	/// <param name="_tyep">どの種類か</param>
	/// <returns>CPU、GPUハンドルとインデックス</returns>
	DescriptorHandle Allocate(HeapType _type);

	// ディスクリプタヒープをセットする関数
	void SetDiscriptor(ID3D12GraphicsCommandList* _cmdList); 

	// 解放する
	void Free(HeapType _type, const DescriptorHandle& _handle);

private:
	// コンストラクタ
	DescriptorManager();

	// コピー禁止
	DescriptorManager(const DescriptorManager& _other) = delete;
	DescriptorManager& operator =(const DescriptorManager& _other) = delete;

private:
	HeapData data[3]{}; // 各ヒープのパラメータを格納した配列(0 = CBV, 1 = RTV, 2 = DSV)
};
