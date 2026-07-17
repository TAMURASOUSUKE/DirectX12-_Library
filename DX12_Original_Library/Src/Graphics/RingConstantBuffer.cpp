#include <cstdint>
#include "../Debug/DebugLogs.h"
#include "GraphicsConstant.h"
#include "GraphicsDevice.h"
#include "GraphicsResourceManager.h"
#include "RingConstantBuffer.h"

void RingConstantBuffer::Initialize(UINT _dataSize)
{
	alignedSize = (_dataSize + 0xff) & ~0xff; // 256バイトへの切り上げ
	DynamicBuffer db{ GraphicsResourceManager::Instance().CreateDynamicBuffer(FRAME_BUFFER_COUNT * MAX_CB_PER_FRAME * alignedSize) }; // 動的なバッファ確保
	if (!db.mappedPtr)
	{
		DEBUG_LOG_ERROR("MapされたCPUPtrがnullで失敗しました\n");
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
	frameCounter = 0;
}

D3D12_GPU_VIRTUAL_ADDRESS RingConstantBuffer::Update(const void* _src, UINT _size)
{
	// データサイズが境界調整済みサイズより大きいと隣のCBデータにはみ出してバグの原因になるのでチェックする
	DEBUG_ASSERT(_size <= alignedSize && "データが境界調整済みサイズより大きいです");
	DEBUG_ASSERT(frameCounter < MAX_CB_PER_FRAME && "1フレームのCB数が上限超過");
	if (_size > alignedSize)
	{
		return 0;
	}

	if (frameCounter >= MAX_CB_PER_FRAME)
	{
		return 0;
	}
	UINT offset{ CalculateOffset() }; // 今のフレームのオフセット
	memcpy(static_cast<uint8_t*>(baseCPUPtr) + offset, _src, _size); // CPUデータをGPUメモリにコピー
	D3D12_GPU_VIRTUAL_ADDRESS addr{ baseGPUVA + offset }; // 同じオフセットのアドレス
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
	UINT slice{ GraphicsDevice::Instance().GetCurrentFrameIndex() * MAX_CB_PER_FRAME + frameCounter };
	return slice * alignedSize; // スライスの位置を計算して256境界に切り上げたオフセットと計算する
}

void RingConstantBuffer::Reset()
{
	frameCounter = 0;
}