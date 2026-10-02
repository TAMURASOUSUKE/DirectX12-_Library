#include <utility>
#include "../External/Common/d3dx12.h"
#include "../Debug/DebugLogs.h"
#include "GraphicsGPUTimingSystem.h"

bool GraphicsGPUTimingSystem::Initialize(ID3D12Device* _device, UINT64 _timestampFrequency)
{
	if (_device == nullptr)
	{
		DEBUG_LOG_ERROR("GPUTimingSystemの初期化に必要なDeviceが存在しません\n");
		return false;
	}

	if (_timestampFrequency == 0)
	{
		DEBUG_LOG_ERROR("GPUTimestampの周波数が0です\n");
		return false;
	}

	if (initialized) return true;

	// 途中で失敗した場合にメンバを半初期化状態にしないためにローカルで作る
	ComPtr<ID3D12QueryHeap> newQueryHeap{};
	ComPtr<ID3D12Resource> newReadbackBuffer{};

	// GPU Timestampを記録するQueryHeapの作成
	D3D12_QUERY_HEAP_DESC queryHeapDesc{};
	queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
	queryHeapDesc.Count = static_cast<UINT>(TOTAL_QUERY_COUNT);
	queryHeapDesc.NodeMask = 0;

	HRESULT result{ _device->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&newQueryHeap)) };
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("GPUTimestamp用QueryHeapの作成に失敗しました\n");
		return false;
	}

	// Query結果をCPUで読み取るためのReadbackBufferを作る
	const UINT64 readbackBufferSize{ static_cast<UINT64>(sizeof(UINT64) * TOTAL_QUERY_COUNT) };
	const CD3DX12_HEAP_PROPERTIES readbackHeap{ D3D12_HEAP_TYPE_READBACK };
	const CD3DX12_RESOURCE_DESC readbackDesc{ CD3DX12_RESOURCE_DESC::Buffer(readbackBufferSize) };

	result = _device->CreateCommittedResource(
		&readbackHeap,
		D3D12_HEAP_FLAG_NONE,
		&readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&newReadbackBuffer));

	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("GPU Timestamp用ReadbackBufferの作成に失敗しました\n");
		return false;
	}

	// 成功判定なので所有権を移す
	queryHeap = std::move(newQueryHeap);
	readbackBuffer = std::move(newReadbackBuffer);

	frameSlots = {};
	lastFrame = {};
	timestampFrequency = _timestampFrequency;
	currentFrameIndex = 0;
	initialized = true;

	return true;

}

void GraphicsGPUTimingSystem::Shutdown()
{
	frameSlots = {};
	lastFrame = {};
	timestampFrequency = 0;
	currentFrameIndex = 0;
	initialized = false;

	readbackBuffer.Reset();
	queryHeap.Reset();
}

bool GraphicsGPUTimingSystem::BeginFrame(ID3D12GraphicsCommandList* _commandList, UINT _frameIndex)
{
	if (!initialized || !queryHeap)
	{
		DEBUG_LOG_ERROR("GPUTimingSystemが初期化されていません\n");
		return false;
	}

	if (!_commandList)
	{
		DEBUG_LOG_ERROR("GPU計測に必要なCommandListが存在しません\n");
		return false;
	}

	if (_frameIndex >= FRAME_BUFFER_COUNT)
	{
		DEBUG_LOG_ERROR("フレームインデックスがダブルバッファのインデックスを超過しています\n");
		return false;
	}

	// 前回同じFrameSlotへ保存したGPU計測結果を読み取る
	if (!ReadCompletedFrame(_frameIndex)) return false;
	currentFrameIndex = _frameIndex;

	// 今回のGPU計測用にFrameSlotの状態を初期化する
	FrameSlotState& frameSlot{ frameSlots[currentFrameIndex] };
	frameSlot = {};

	// Scope0へGPUフレーム全体の開始Timestampを記録する(全体はSegment0)
	const UINT beginQueryIndex{ GetQueryIndex(currentFrameIndex, 0, 0, false) };
	_commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, beginQueryIndex);
	frameSlot.active[0] = true;
	return true;
}

bool GraphicsGPUTimingSystem::BeginPass(ID3D12GraphicsCommandList* _commandList, GraphicsPass _pass)
{
	if (!initialized || !queryHeap)
	{
		DEBUG_LOG_ERROR("GPUTimingSystemが初期化されていません\n");
		return false;
	}

	if (!_commandList)
	{
		DEBUG_LOG_ERROR("GPU計測に必要なCommandListが存在しません\n");
		return false;
	}

	const std::size_t passIndex{ static_cast<std::size_t>(_pass) };
	if (passIndex >= PASS_COUNT)
	{
		DEBUG_LOG_ERROR("GPU計測に不正なGraphicsPassが指定されました\n");
		return false;
	}

	// Scope0はGPUフレームの全体なのでPass番号に1を足す
	const std::size_t scopeIndex{ passIndex + 1 };
	FrameSlotState& frameSlot{ frameSlots[currentFrameIndex] };

	// BeginFrameが呼ばれていない場合はPass計測を開始しない
	if (!frameSlot.active[0])
	{
		DEBUG_LOG_ERROR("GPUフレーム計測が開始されていません\n");
		return false;
	}

	// 同じPassのBegin二重呼び出しを防ぐ
	if (frameSlot.active[scopeIndex])
	{
		DEBUG_LOG_ERROR("同じGPUPassが終了前に再度開始されました\n");
		return false;
	}

	// 記録済み区間数を次のSegment番号として使う
	const std::size_t segmentIndex{ frameSlot.recordedSegmentCount[scopeIndex] };
	if (segmentIndex >= MAX_SEGMENT_COUNT_PER_SCOPE)
	{
		DEBUG_LOG_ERROR("同じGPUPassの計測区間数が上限を超過しました\n");
		return false;
	}

	const UINT beginQueryIndex{ GetQueryIndex(currentFrameIndex, scopeIndex, segmentIndex, false) };
	_commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, beginQueryIndex);
	frameSlot.active[scopeIndex] = true;
	return true;
}

bool GraphicsGPUTimingSystem::EndPass(ID3D12GraphicsCommandList* _commandList, GraphicsPass _pass)
{
	if (!initialized || !queryHeap)
	{
		DEBUG_LOG_ERROR("GPUTimingSystemが初期化されていません\n");
		return false;
	}

	if (!_commandList)
	{
		DEBUG_LOG_ERROR("GPU計測に必要なCommandListが存在しません\n");
		return false;
	}

	const std::size_t passIndex{ static_cast<std::size_t>(_pass) };
	if (passIndex >= PASS_COUNT)
	{
		DEBUG_LOG_ERROR("GPU計測に不正なGraphicsPassが指定されました\n");
		return false;
	}

	// Scope0はGPUフレームの全体なのでPass番号に1を足す
	const std::size_t scopeIndex{ passIndex + 1 };
	FrameSlotState& frameSlot{ frameSlots[currentFrameIndex] };

	// BeginPassされていないPassは終了できない
	if (!frameSlot.active[scopeIndex])
	{
		DEBUG_LOG_ERROR("開始されていないGPU Pass計測を終了しようとしました\n");
		return false;
	}

	// BeginPassの時と同じSegmentを使う
	const std::size_t segmentIndex{ frameSlot.recordedSegmentCount[scopeIndex] };
	if (segmentIndex >= MAX_SEGMENT_COUNT_PER_SCOPE)
	{
		DEBUG_LOG_ERROR("GPU Passの終了先Segmentが範囲外です\n");
		return false;
	}

	const UINT endQueryIndex{ GetQueryIndex(currentFrameIndex, scopeIndex, segmentIndex, true) };
	_commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endQueryIndex);

	frameSlot.active[scopeIndex] = false; // 非有効状態
	frameSlot.recordedSegmentCount[scopeIndex]++; // 書き込まれた状態を増やす

	return true;
}

bool GraphicsGPUTimingSystem::EndFrame(ID3D12GraphicsCommandList* _commandList)
{
	if (!initialized || !queryHeap || !readbackBuffer)
	{
		DEBUG_LOG_ERROR("GPUTimingSystemが初期化されていません\n");
		return false;
	}

	if (!_commandList)
	{
		DEBUG_LOG_ERROR("GPU計測に必要なCommandListが存在しません\n");
		return false;
	}

	FrameSlotState& frameSlot{ frameSlots[currentFrameIndex] };

	// BeginFrameされていないGPUフレームは終了できない
	if (!frameSlot.active[0])
	{
		DEBUG_LOG_ERROR("開始されていないGPUフレーム計測を終了しようとしました\n");
		return false;
	}

	// 終了されていないPassがある場合は計測結果を確定できない
	for (std::size_t scopeIndex = 1; scopeIndex < SCOPE_COUNT; scopeIndex++)
	{
		if (frameSlot.active[scopeIndex])
		{
			DEBUG_LOG_ERROR("終了されていないGPU Pass計測があります\n");
			return false;
		}
	}

	// Scope0へGPUフレーム全体の終了timestampを記録する(全体なのでSegmentは0)
	const UINT endQueryIndex{ GetQueryIndex(currentFrameIndex, 0, 0, true) };
	_commandList->EndQuery(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, endQueryIndex);

	frameSlot.active[0] = false;
	frameSlot.recordedSegmentCount[0] = 1; // 全体計測分を追加するので1を入れる

	// 開始と終了が記録されたScopeだけReadbackBufferへResolveする
	for (std::size_t scopeIndex = 0; scopeIndex < SCOPE_COUNT; scopeIndex++)
	{
		const std::size_t segmentCount{ frameSlot.recordedSegmentCount[scopeIndex] };
		if (segmentCount == 0) continue;

		const UINT beginQueryIndex{ GetQueryIndex(currentFrameIndex, scopeIndex, 0, false) };
		const UINT queryCount{ static_cast<UINT>(segmentCount * QUERY_COUNT_PER_SEGMENT) };
		const UINT64 destinationOffset{ static_cast<UINT64>(beginQueryIndex) * sizeof(UINT64) };

		// クエリヒープ内のCPUが読めないデータを変換する
		_commandList->ResolveQueryData(queryHeap.Get(), D3D12_QUERY_TYPE_TIMESTAMP, beginQueryIndex, queryCount, readbackBuffer.Get(), destinationOffset);
	}

	// resolve命令を記録したためGPU完了後に読み取れる
	frameSlot.pendingReadback = true;
	return true;

}

UINT GraphicsGPUTimingSystem::GetQueryIndex(UINT _frameIndex, std::size_t _scopeIndex, std::size_t _segmentIndex, bool _isEnd) const
{
	DEBUG_ASSERT(_frameIndex < FRAME_BUFFER_COUNT);
	DEBUG_ASSERT(_scopeIndex < SCOPE_COUNT);
	DEBUG_ASSERT(_segmentIndex < MAX_SEGMENT_COUNT_PER_SCOPE);

	const std::size_t frameOffset{ static_cast<std::size_t>(_frameIndex) * QUERY_COUNT_PER_FRAME }; // FrameSlotの先頭
	const std::size_t segmentOffset{ static_cast<std::size_t>(_segmentIndex * QUERY_COUNT_PER_SEGMENT) }; // Segmentの先頭
	const std::size_t scopeOffset{ _scopeIndex * QUERY_COUNT_PER_SCOPE }; // スコープの先頭
	const std::size_t timestampOffset{ _isEnd ? 1u : 0u }; // 開始終了Offset開始なら0終了なら1

	return static_cast<UINT>(frameOffset + scopeOffset + segmentOffset + timestampOffset);
}

bool GraphicsGPUTimingSystem::ReadCompletedFrame(UINT _frameIndex)
{
	if (!initialized)
	{
		DEBUG_LOG_ERROR("初期化されていない状態で呼び出されました\n");
		return false;
	}

	if (!readbackBuffer)
	{
		DEBUG_LOG_ERROR("不正な読み取り用バッファです\n");
		return false;
	}

	if (_frameIndex >= FRAME_BUFFER_COUNT)
	{
		DEBUG_LOG_ERROR("フレームインデックスがダブルバッファのインデックスを超過しています\n");
		return false;
	}

	if (timestampFrequency == 0)
	{
		DEBUG_LOG_ERROR("タイムスタンプの周波数が設定されていません\n");
		return false;
	}

	const UINT firstQueryIndex{ GetQueryIndex(_frameIndex, 0, 0, false) }; // 指定FrameSlotの先頭Query番号
	
	// byte位置への変換
	const SIZE_T beginByte{ static_cast<SIZE_T>(firstQueryIndex) * sizeof(UINT64) }; // 開始バイト
	const SIZE_T readbackByte{ QUERY_COUNT_PER_FRAME * sizeof(UINT64) }; // 読み取りバイト数
	const SIZE_T endByte{ beginByte + readbackByte }; // 終了バイト

	// メモリ範囲の設定
	D3D12_RANGE range{};
	range.Begin = beginByte;
	range.End = endByte;

	FrameSlotState& frameSlot{ frameSlots[_frameIndex] };
	if (!frameSlot.pendingReadback) return true; // 初回のスロットにはまだ結果がないのでそのまま返す

	void* mappedPtr{ nullptr };
	HRESULT result{ readbackBuffer->Map(0, &range,  &mappedPtr) };
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result) || mappedPtr == nullptr)
	{
		DEBUG_LOG_ERROR("GPU Timestamp用ReadbackBufferのMapに失敗しました\n");
		return false;
	}

	// 完成値を作り直す
	lastFrame = {};

	// タイムスタンプを変換
	const UINT64* timestampData{ static_cast<const UINT64*>(mappedPtr) };

	// 各スコープの全segmentを合計する
	for (std::size_t scopeIndex = 0; scopeIndex < SCOPE_COUNT; scopeIndex++)
	{
		const std::size_t segmentCount{ frameSlot.recordedSegmentCount[scopeIndex] };
		if (segmentCount == 0) continue;

		double totalMilliseconds{ 0.0 }; // 合計値
		bool hasValidSegment{ false };

		// ここからSegment分回して合計する
		for (std::size_t segmentIndex = 0; segmentIndex < segmentCount; segmentIndex++)
		{
			const UINT beginIndex{ GetQueryIndex(_frameIndex, scopeIndex, segmentIndex, false) };
			const UINT endIndex{ GetQueryIndex(_frameIndex, scopeIndex, segmentIndex,true) };

			const UINT64 beginTimestamp{ timestampData[beginIndex] };
			const UINT64 endTimestamp{ timestampData[endIndex] };

			if (beginTimestamp > endTimestamp) continue;

			// GPUTick->ミリ秒への変換して合計する
			totalMilliseconds += static_cast<double>(endTimestamp - beginTimestamp) * 1000.0 / static_cast<double>(timestampFrequency);
			hasValidSegment = true;
		}
		if (!hasValidSegment) continue;

		if (scopeIndex == 0)
		{
			// Scope 0はGPUフレーム全体
			lastFrame.total.milliseconds = totalMilliseconds;
			lastFrame.total.valid = true;
		}
		else
		{
			// Scope 1以降はRenderPassなので1を引く
			const std::size_t passIndex{ scopeIndex - 1 };

			lastFrame.passes[passIndex].milliseconds = totalMilliseconds;
			lastFrame.passes[passIndex].valid = true;
		}
	}

	// 最後にUnmapsする
	const D3D12_RANGE writtenRange{ 0, 0 };
	readbackBuffer->Unmap(0, &writtenRange);
	frameSlots[_frameIndex].pendingReadback = false;
	return true;
}
