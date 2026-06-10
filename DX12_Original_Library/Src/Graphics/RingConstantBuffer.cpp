#include <cstdint>
#include <cassert>
#include "GraphicsConstant.h"
#include "GraphicsDevice.h"
#include "ResourceManager.h"
#include "RingConstantBuffer.h"

void RingConstantBuffer::Initialize(UINT _dataSize)
{
	alignedSize = (_dataSize + 0xff) & ~0xff; // 256バイトへの切り上げ
	DynamicBuffer db{ ResourceManager::Instance().CreateDynamicBuffer(FRAME_BUFFER_COUNT * alignedSize) }; // 動的なバッファ確保
	if (!db.mappedPtr) return; // mapされたCPUptrを確認してnullであれば失敗判定

	// メンバへ渡す
	resource = db.resource; // リソースオブジェクト
	baseCPUPtr = db.mappedPtr; // マップしたCPUポインタ
	baseGPUVA = resource->GetGPUVirtualAddress(); // ベースの仮想アドレスをキャッシュして保持
}

void RingConstantBuffer::Update(const void* _src, UINT _size)
{
	// 一旦今後作るDebugクラスようにassertにしておく
	// データサイズが境界調整済みサイズより大きいと隣のCBデータにはみ出してバグの原因になるのでチェックする
	assert(_size <= alignedSize && "データが境界調整済みサイズより大きいです");

	memcpy(static_cast<uint8_t*>(baseCPUPtr) + CalculatOffset(), _src, _size); // CPUデータをGPUメモリにコピー

}

D3D12_GPU_VIRTUAL_ADDRESS RingConstantBuffer::GetCurrentVertualAddress() const
{
	// ベースのGPUの仮想アドレス + offsetを返す
	return baseGPUVA + CalculatOffset();
}

UINT RingConstantBuffer::CalculatOffset() const
{
	// 現在のフレーム数と256境界に調整済みのデータサイズを掛け合わせたオフセットを計算する
	return GraphicsDevice::Instance().GetCurrentFrameIndex() * alignedSize;
}