#pragma once
#include <d3d12.h>
#include <cstddef>
#include <vector>
#include "GraphicsType.h"
#include "RingConstantBuffer.h"


// Compute実行要求の登録とCommandListへの記録を担当する
class ComputeSystem
{
public:
	ComputeSystem() = default;
	~ComputeSystem() = default;

	ComputeSystem(const ComputeSystem& _other) = delete;
	ComputeSystem& operator=(const ComputeSystem& _other) = delete;

	// 初期化
	bool Setup(ID3D12Device* _device);

	// 終了関数
	void Shutdown();

	// 実行設定と定数データを内部へコピーして記録する
	bool RegisterDispatch(const ComputeDispatchDesc& _desc, const void* _data = nullptr, std::size_t _parameterSize = 0);

	// Upload記録時に登録順で遷移状態・接続・Dispatchを記録
	bool Flush(ID3D12GraphicsCommandList* _cmd);

	// フレーム開始時に要求数と定数バッファの書き込み位置を戻す
	void Reset();

	// 未実行の要求があるかどうか
	bool HasPendingDispatch() const { return !commands.empty(); }

private:
	ID3D12Device* device{ nullptr }; // デバイス(非所有)
	std::vector<ComputeDispatchCommand> commands{}; // 実行要求の配列
	std::vector<D3D12_RESOURCE_BARRIER> transitionAllResourceBarriers{}; // Dispatch後にTextureをAllResoruceStateに戻すためのBarreir
	RingConstantBuffer ringCBV{}; // 要求ごとの定数データを格納する

	DescriptorHandle nullSRV{}; // 未設定の入力スロットへ設定するnullSRV
	DescriptorHandle nullUAV{}; // 未設定の入力スロットへ設定するnullUAV

	bool isInitialize{ false }; // 初期化されたか
	bool isFlush{ false }; // Flushを実行したか
};
