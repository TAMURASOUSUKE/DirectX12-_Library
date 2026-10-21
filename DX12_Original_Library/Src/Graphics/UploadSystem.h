#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstddef>
#include <vector>
#include <span>
#include "UploadType.h"

using Microsoft::WRL::ComPtr;

// GPUリソースの転送をまとめて管理する
class UploadSystem
{
public:
	UploadSystem() = default;
	~UploadSystem() = default;

	// コピー禁止
	UploadSystem(const UploadSystem& _other) = delete;
	UploadSystem& operator=(const UploadSystem& _other) = delete;

	// 転送用メモリの基本サイズと確保できる総量を設定する
	bool Setup(ID3D12Device* _device, UINT64 _pageSize, UINT64 _maxUploadBytes);

	// GPUの完了を待った後に呼び出す
	void Shutdown();

	// Buffer全体の初期データを内部へコピーして転送予約する
	bool EnqueueBuffer(ID3D12Resource* _destination, const void* _sourceData, UINT64 _sizeInBytes, D3D12_RESOURCE_STATES _beforeState, D3D12_RESOURCE_STATES _afterState);

	// Texture全体の初期データを内部へコピーして転送予約する
	bool EnqueueTexture(ID3D12Resource* _destination, std::span<const D3D12_SUBRESOURCE_DATA> _subresources, D3D12_RESOURCE_STATES _beforeState, D3D12_RESOURCE_STATES _afterState);

	// 予約済みのコピーと状態遷移をCommandListへ記録する
	bool RecordPendingUploads(ID3D12GraphicsCommandList* _commandList);

	// 記録した命令の実行後Signalに成功したFence値を関連付ける
	bool CommitSubmittedUploads(UINT64 _fenceValue);

	// GPU完了済みBatchを回収して転送用メモリを再利用可能にする
	void CollectCompletedUploads(UINT64 _completedFenceValue);

	// まだCommandListへ記録していない予約があるか
	bool HasPendingUploads() const;

	// 記録済みだがまだFenceと関連付けていない転送があるか
	bool HasRecordedUploads() const;

private:
	// 予約を追加するBatchを取得するなければ空きBatchを利用する
	std::size_t AcquirePendingBatch();

	// 指定サイズと配置境界を満たす転送用メモリを確保する
	bool AllocateUpload(UINT64 _sizeInBytes, UINT64 _alignment, UploadAllocation& _outAllocation);

	// 再利用できるPageがなければ新しいUpload Heapを作成する
	bool CreateUploadPage(UINT64 _minimumSize, std::size_t& _outPageIndex);

	// GPU完了済みBatchのデータを消しPageを再利用可能にする
	void RecycleBatch(std::size_t _batchIndex);

private:
	ComPtr<ID3D12Device> device{}; // デバイスの参照を保持

	std::vector<UploadPage> pages{}; // Pageの実体配列
	std::vector<UploadBatch> batches{}; // Batchの実体配列

	std::size_t pendingBatchIndex{ INVALID_UPLOAD_INDEX }; // 予約しているbatchのインデックス
	std::size_t recordedBatchIndex{ INVALID_UPLOAD_INDEX }; // 記録ずみかつ命令送信していないインデックス

	UINT64 pageSize{ 0 }; // ページの大きさ
	UINT64 maxUploadBytes{ 0 }; // 送る際の最大容量
	UINT64 allocatedUploadBytes{ 0 }; // 確保したリソースの合計サイズ

	bool initialized{ false }; // 初期化されたか
};
