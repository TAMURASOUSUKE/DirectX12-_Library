#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstddef>
#include <vector>
using Microsoft::WRL::ComPtr;

// リソースを転送する際に必要な列挙体や構造体を定義する

// 転送するバッファのインデックスの無効値
constexpr std::size_t INVALID_UPLOAD_INDEX{ static_cast<std::size_t>(-1) };

// 転送するリソースの種類
enum class UploadType
{
	Buffer, // バッファ
	Texture, // テクスチャ
};

// まとめた転送命令の進行
enum class BatchState
{
	Free, // 再利用可能
	Pending, // 待機状態
	Recorded, // 命令を記録しているがFenceはまだ
	InFlight, // GPUで実行中
};

// 複数のデータを詰めて使用するUploadHeapのBuffer
struct UploadPage
{
	ComPtr<ID3D12Resource> resource{}; // リソース本体
	std::byte* mappedData{ nullptr }; // Mapで得た書き込み先
	UINT64 capacity{ 0 }; // 確保している容量
	UINT64 usedBytes{ 0 }; // 使用済みの範囲
	bool inUse{ false }; // いずれかのBatchが使用しているか
};

// UploadPage内で確保した範囲
struct UploadAllocation
{
	std::size_t pageIndex{ INVALID_UPLOAD_INDEX }; // ページの番号
	UINT64 offset{ 0 }; // Page先頭からの位置
	UINT64 sizeInBytes{ 0 }; // 確保したサイズ
};

// Bufferのコピーに必要な情報
struct BufferCopy
{
	std::size_t pageIndex{ INVALID_UPLOAD_INDEX }; // ページの番号
	UINT64 sourceOffset{ 0 }; // Sourceの先頭からの位置
	UINT64 sizeInBytes{ 0 }; // サイズ
};

// TextureのSubResource一つ分のコピー情報
struct TextureCopy
{
	std::size_t pageIndex{ INVALID_UPLOAD_INDEX }; // ページ番号
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{}; // 転送用Buffer内での配置、行間隔、画像サイズ
	UINT subresourceIndex{ 0 }; // コピー先のMipなどの番号
};

// リソース一つ分の転送要求
struct UploadRequest
{
	ComPtr<ID3D12Resource> destination{}; // 転送が終わる前に元のハンドルが破棄されても本体を保持できるようにする
	UploadType type{ UploadType::Buffer }; // リソースの種類
	D3D12_RESOURCE_STATES beforBuffer{ D3D12_RESOURCE_STATE_COMMON }; // リソースの遷移前の状態
	D3D12_RESOURCE_STATES afterBuffer{ D3D12_RESOURCE_STATE_COMMON }; // リソースの遷移後の状態
	std::size_t firstCopyIndex{ 0 }; // 最初にコピーした位置のインデックス
	std::size_t copyCount{ 0 }; // いくつコピーするか
};

// 同じFenceで寿命を管理する転送のまとまり
struct UploadBatch
{
	std::vector<UploadRequest> requests{}; // 転送要求
	std::vector<BufferCopy> bufferCopies{}; // バッファのコピー情報
	std::vector<TextureCopy> textureCopies{}; // テクスチャのコピー情報
	std::vector<std::size_t> pageIndices{}; 	// このBatchが使用しているPageの番号
	UINT64 fenceValue{ 0 }; // フェンス値
	BatchState state{ BatchState::Free }; // 転送の進行状況
};
