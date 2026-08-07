#include <cstdint>
#include <limits>
#include <cstring>
#include "../Debug/DebugLogs.h"
#include "GraphicsConstant.h"
#include "GraphicsDevice.h"
#include "GraphicsResourceManager.h"
#include "RingConstantBuffer.h"

void RingConstantBuffer::Setup(UINT _dataSize, UINT _maxUpadatePerFrame)
{
	if (_dataSize == 0 || _maxUpadatePerFrame == 0)
	{
		DEBUG_LOG_ERROR("RingConstantBufferの初期化引数が不正です\n");
		return;
	}

	alignedSize = (_dataSize + 0xff) & ~0xff; // 256バイトへの切り上げ
	maxUpdatesPerFrame = _maxUpadatePerFrame;

	// バックバッファごとに独立した領域を用意する
	const UINT64 totalSize{ static_cast<UINT64>(FRAME_BUFFER_COUNT) * static_cast<UINT64>(maxUpdatesPerFrame) * static_cast<UINT64>(alignedSize) };

	// CreateDynamicBufferがUINTを受け取るための範囲設定
	if (totalSize > static_cast<UINT64>((std::numeric_limits<UINT>::max)()))
	{
		DEBUG_LOG_ERROR("RingConstantBufferの確保サイズがUINT上限を超えています\n");
		alignedSize = 0;
		maxUpdatesPerFrame = 0;
		return;
	}

	DynamicBuffer db{ GraphicsResourceManager::Instance().CreateDynamicBuffer(static_cast<UINT>(totalSize)) }; // 動的なバッファ確保
	if (!db.mappedPtr || !db.resource)
	{
		DEBUG_LOG_ERROR("RingConstantBufferの作成に失敗しました\n");
		return; // mapされたCPUptrを確認してnullであれば失敗判定
	}

	// メンバへ渡す
	resource = db.resource; // リソースオブジェクト
	baseCPUPtr = db.mappedPtr; // マップしたCPUポインタ
	baseGPUVA = resource->GetGPUVirtualAddress(); // ベースの仮想アドレスをキャッシュして保持
}

void RingConstantBuffer::Shutdown()
{
	resource.Reset();
	baseCPUPtr = nullptr;
	baseGPUVA = 0;
	alignedSize = 0;
	maxUpdatesPerFrame = 0;
	frameCounter = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS RingConstantBuffer::Update(const void* _src, UINT _size)
{
	// データサイズが境界調整済みサイズより大きいと隣のCBデータにはみ出してバグの原因になるのでチェックする
	DEBUG_ASSERT(_size <= alignedSize && "データが境界調整済みサイズより大きいです");
	DEBUG_ASSERT(frameCounter < maxUpdatesPerFrame && "1フレームのCB数が上限超過");

	if (!_src || !resource || !baseCPUPtr || _size > alignedSize || frameCounter >= maxUpdatesPerFrame)
	{
		return 0;
	}

	const UINT offset{ CalculateOffset() }; // 今のフレームのオフセット
	std::memcpy(static_cast<uint8_t*>(baseCPUPtr) + offset, _src, _size); // CPUデータをGPUメモリにコピー
	const D3D12_GPU_VIRTUAL_ADDRESS addr{ baseGPUVA + offset }; // 同じオフセットのアドレス
	frameCounter++; // 次の描画へ進める
	return addr;

}

D3D12_GPU_VIRTUAL_ADDRESS RingConstantBuffer::GetCurrentVirtualAddress() const
{
	// ベースのGPUの仮想アドレス + offsetを返す
	return baseGPUVA + CalculateOffset();
}

UINT RingConstantBuffer::CalculateOffset() const
{
	UINT slice{ GraphicsDevice::Instance().GetCurrentFrameIndex() * maxUpdatesPerFrame + frameCounter };
	return slice * alignedSize; // スライスの位置を計算して256境界に切り上げたオフセットと計算する
}

void RingConstantBuffer::Reset()
{
	frameCounter = 0;
}