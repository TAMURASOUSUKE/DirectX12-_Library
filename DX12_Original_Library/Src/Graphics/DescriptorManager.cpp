#include "../Debug/DebugLogs.h"
#include "GraphicsConstant.h"
#include "DescriptorManager.h"

DescriptorManager::DescriptorManager()
{
	// ヒープの設定を行う(生成は明示的に関数で呼び出すが内部設定はコンストラクタで暗黙的に行う)

	// SRV/CBV/UAV用
	data[0].type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV; // GPU可視のディスクリプタに設定
	data[0].flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE; // GPU可視のためSHADER_VISIBLE
	data[0].slotCount = SHADERVISIBLE_SLOT_COUNT; // GPU可視用のディスクリプタを定数分用意

	// RTV用
	data[1].type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // RTVに設定
	data[1].flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // GPU非可視のためNONE
	data[1].slotCount = RTV_SLOT_COUNT; // RTVを定数分確保

	// DSV用
	data[2].type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV; // DSVに設定
	data[2].flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; // GPU非可視のためNONE
	data[2].slotCount = DSV_SLOT_COUNT; // DSVを定数分用意
}

// シングルトン内部
DescriptorManager& DescriptorManager::Instance()
{
	static DescriptorManager instance;
	return instance;
}

void DescriptorManager::Setup(ID3D12Device* _device)
{
	HRESULT result{}; // 生成結果等を確認するための変数
	
	// 各ヒープに設定を詰めていく
	for (int i = 0; i < HEAP_COUNT; i++)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc{}; // ヒープの設定を行う構造体
		desc.Type = data[i].type; // 各ヒープのtypeを格納
		desc.Flags = data[i].flags; // 各ヒープのflagを格納
		desc.NumDescriptors = data[i].slotCount; // 各ヒープのスロット数を格納
		
		// 実際にヒープを生成していく
		result = _device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&data[i].heap));
		DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
		if (FAILED(result)) return; // 失敗したら終了

		// ビュー一つ分のサイズを取得して格納
		data[i].descriptorSize = _device->GetDescriptorHandleIncrementSize(data[i].type);

		// 初期化
		for (UINT j = 0; j < data[i].slotCount; j++)
		{
			data[i].freeList.push(j);
		}
	}
}

// 終了処理
void DescriptorManager::Shutdown()
{
	for (int i = 0; i < HEAP_COUNT; i++)
	{
		data[i].heap.Reset();
		// 空でないなら全て消す
		while (!data[i].freeList.empty())
		{
			data[i].freeList.pop();
		}
	}
}

// ヒープに割り当てる関数
DescriptorHandle DescriptorManager::Allocate(HeapType _type)
{
	auto& h{ data[static_cast<int>(_type)] }; // 指定されたtypeのheapを取り出す

	DEBUG_ASSERT(!h.freeList.empty());
	if (h.freeList.empty()) return DescriptorHandle{}; // 配列が空ならデフォルトを返す

	UINT index{ h.freeList.top() }; // スタックの頭にある値をインデックスとして扱う
	h.freeList.pop(); // 取り出したので消す


	DescriptorHandle handle{};
	handle.index = index;

	// CPUハンドルを計算する(先頭 + index * size)
	handle.cpu = h.heap->GetCPUDescriptorHandleForHeapStart(); // CPUの先頭ハンドルを取得
	handle.cpu.ptr += index * h.descriptorSize;

	// ShaderVisibleでのみGPUハンドルを取得するようにする
	if (h.flags == D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE)
	{
		handle.gpu = h.heap->GetGPUDescriptorHandleForHeapStart(); // GPUの先頭ハンドルを取得
		handle.gpu.ptr += index * h.descriptorSize;
	}

	return handle;
}

// ディスクリプタヒープをセットする
void DescriptorManager::SetDiscriptor(ID3D12GraphicsCommandList* _cmdList)
{
	_cmdList->SetDescriptorHeaps(1, data[0].heap.GetAddressOf()); // ComPtrが内部で持っているポインタのアドレスを返す
}

// ハンドルを戻す
void DescriptorManager::Free(HeapType _type, const DescriptorHandle& _handle)
{
	if (!_handle.IsValid()) return; // 無効はfreelistを汚すのではじく(失敗リソースの後でfreeすることもあるのでここはスキップ)
	DEBUG_ASSERT(_handle.index < data[static_cast<int>(_type)].slotCount); // ハンドルが範囲内かチェック
	// インデックスをスタックに戻す
	data[static_cast<int>(_type)].freeList.push(_handle.index);
}

