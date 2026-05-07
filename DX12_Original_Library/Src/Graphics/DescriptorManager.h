#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <stack>
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;

#pragma comment(lib, "d3d12.lib")
// ShaderVisibleのSRV,CBV,UAV、CPUOnlyのRTV、DSV用の3つのヒープを管理するクラス
class DescriptorManager
{
public:
	// シングルトン化
	static DescriptorManager& Instance();
	// デフォルトデストラクタ
	~DescriptorManager() = default;

	// 初期化
	void Initialize(ID3D12Device* _device);
	// 終了処理
	void Shutdown();

	/// <summary>
	/// ヒープに割り当てる関数
	/// </summary>
	/// <param name="_tyep">どの種類か</param>
	/// <returns>CPU、GPUハンドルとインデックス</returns>
	DescriptorHandle Allocate(HeapType _type);

	// 解放する
	void Free(HeapType _type, const DescriptorHandle& _handle);

private:
	// コンストラクタ
	DescriptorManager() = default;

	// コピー禁止
	DescriptorManager(const DescriptorManager& _other) = delete;
	DescriptorManager& operator =(const DescriptorManager& _other) = delete;

private:
	ComPtr<ID3D12DescriptorHeap> rtvHeap; // RTV用のヒープ
	ComPtr<ID3D12DescriptorHeap> dsvHeap; // DSV用のヒープ
	ComPtr<ID3D12DescriptorHeap> shaderVisibleHeap; // SRV/CBV/UAV用のヒープ
	UINT rtvDescriptorSize{ 0 }; // RTV一つのサイズ
	UINT dsvDescriptorSize{ 0 }; // DSV一つのサイズ
	UINT shaderVisibleDescriptorSize{ 0 }; // GPU可視(SRV/CBV/UAV)の一つのサイズ
	std::stack<UINT> rtvFreeList; // rtvの空きスロット番号のスタック
	std::stack<UINT> dsvFreeList; // dsvの空きスロット番号のスタック
	std::stack<UINT> shaderVisibleFreeList; // GPU可視の空きスロット番号のスタック
};