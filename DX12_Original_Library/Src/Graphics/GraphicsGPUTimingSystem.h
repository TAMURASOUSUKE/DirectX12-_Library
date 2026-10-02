#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <d3d12.h>
#include <wrl/client.h>
#include "GraphicsConstant.h"
#include "GraphicsMetrics.h"
using Microsoft::WRL::ComPtr;

// GPUでの実行時間を計算、取得するためのシステム
class GraphicsGPUTimingSystem
{
public:
	GraphicsGPUTimingSystem() = default;
	~GraphicsGPUTimingSystem() = default;

	GraphicsGPUTimingSystem(const GraphicsGPUTimingSystem& _other) = delete;
	GraphicsGPUTimingSystem& operator=(const GraphicsGPUTimingSystem& _other) = delete;

	// QueryHeapとReadbackBufferを作成する
	bool Initialize(ID3D12Device* _device, UINT64 _timestampFrequency);

	// 終了処理
	void Shutdown();

	// 完了済みの同一FrameSlotを読み取って新しいGPUフレーム計測を開始する
	bool BeginFrame(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex);

	// 指定RenderPassの開始Timestampを記録する
	bool BeginPass(ID3D12GraphicsCommandList* _commandList, GraphicsPass _pass);

	// 指定RenderPassの終了Timestampを記録する
	bool EndPass(ID3D12GraphicsCommandList* _commandList, GraphicsPass _pass);

	// GPUフレーム全体の終了とReadbackへのResolveを記録する
	bool EndFrame(ID3D12GraphicsCommandList* _commandList);

	// 完了済みの直近GPU計測結果を取得する
	const GraphicsGPUTimingFrame& GetLastFrame() const { return lastFrame; }

private:
	static constexpr std::size_t PASS_COUNT{ static_cast<std::size_t>(GraphicsPass::Count) }; // パスの数
	static constexpr std::size_t SCOPE_COUNT{ PASS_COUNT + 1 }; // 0番をGPUフレーム全体、1番以降を各Passにする
	static constexpr std::size_t MAX_SEGMENT_COUNT_PER_SCOPE{ 8 }; // 同じScopeを1フレーム内で計測できる最大区間数
	static constexpr std::size_t QUERY_COUNT_PER_SEGMENT{ 2 }; // 一つの計測区間に必要なTimestamp数
	static constexpr std::size_t QUERY_COUNT_PER_SCOPE{ MAX_SEGMENT_COUNT_PER_SCOPE * QUERY_COUNT_PER_SEGMENT }; // 一つのScopeが確保するQuery数
	static constexpr std::size_t QUERY_COUNT_PER_FRAME{ SCOPE_COUNT * QUERY_COUNT_PER_SCOPE }; // 1フレームのGPU計測に必要なQuery数
	static constexpr std::size_t TOTAL_QUERY_COUNT{ QUERY_COUNT_PER_FRAME * FRAME_BUFFER_COUNT }; // QueryHeap全体で確保するQuery数
	
	// 各FrameSlotでの計測命令の状態記録
	struct FrameSlotState
	{
		// そのScopeごとに開始・終了がそろった区間数
		std::array<std::size_t, SCOPE_COUNT> recordedSegmentCount{};

		// Begin後、まだEndされていないか
		std::array<bool, SCOPE_COUNT> active{};

		// Resolve済みで次回利用時に読み取れるか
		bool pendingReadback{ false };
	};

	// ScopeとFrameSlotからQueryHeap上の添字を作る
	UINT GetQueryIndex(UINT _frameIndex, std::size_t _scopeIndex, std::size_t _segmentIndex, bool _isEnd) const;

	// GPU完了済みFrameSlotから結果を読み取る
	bool ReadCompletedFrame(UINT _frameIndex);

private:
	ComPtr<ID3D12QueryHeap> queryHeap{};
	ComPtr<ID3D12Resource> readbackBuffer{};

	std::array<FrameSlotState, FRAME_BUFFER_COUNT> frameSlots{}; // バッファ分の状態記録用配列

	GraphicsGPUTimingFrame lastFrame{}; // 最後の計測結果

	UINT64 timestampFrequency{ 0 }; // タイムスタンプの周波数
	UINT currentFrameIndex{ 0 }; // 現在のフレームインデックス
	bool initialized{ false }; // 初期化済みかどうか
};
