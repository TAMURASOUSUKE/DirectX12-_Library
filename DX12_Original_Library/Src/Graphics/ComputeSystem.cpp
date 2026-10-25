#include <new>
#include "DescriptorManager.h"
#include "../Debug/DebugLogs.h"
#include "ComputeSystem.h"

bool ComputeSystem::Setup(ID3D12Device* _device)
{
	if (isInitialize)
	{
		DEBUG_LOG_ERROR("すでに初期化されています\n");
		return false;
	}

	if (!_device)
	{
		DEBUG_LOG_ERROR("渡されたDeviceが不正です\n");
		return false;
	}

	// 再確保を避けるためあらかじめ上限分を先に確保
	try
	{
		commands.reserve(MAX_COMPUTE_DISPATCH_COUNT);
	}
	catch (std::bad_alloc&)
	{
		DEBUG_LOG_ERROR("Compute実行要求のメモリ確保に失敗しました\n");
		Shutdown();
		return false;
	}

	// フレームごとに独立した領域へデータを書き込むためにringを初期化
	if (!ringCBV.Setup(static_cast<UINT>(MAX_COMPUTE_PARAMETER_SIZE), static_cast<UINT>(MAX_COMPUTE_DISPATCH_COUNT))) // 実行要求分の最大数更新する
	{
		DEBUG_LOG_ERROR("Compute用定数バッファの初期化に失敗しました\n");
		Shutdown();
		return false;
	}

	DescriptorManager& descriptorManager{ DescriptorManager::Instance() };

	// 未入力のスロットへ設定するNullSRVの置き場所を確保
	nullSRV = descriptorManager.Allocate(HeapType::CBV_SRV_UAV);
	if (!nullSRV.IsValid())
	{
		DEBUG_LOG_ERROR("Compute用Null SRVの確保に失敗しました\n");
		Shutdown();
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = 1;
	srvDesc.Buffer.StructureByteStride = sizeof(float);
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	// リソース本体を指定しないことでNullDescriptorを作成する
	_device->CreateShaderResourceView(nullptr, &srvDesc, nullSRV.cpu);

	// 未入力のスロットへ設定するNullUAVの置き場所を確保
	nullUAV = descriptorManager.Allocate(HeapType::CBV_SRV_UAV);
	if (!nullUAV.IsValid())
	{
		DEBUG_LOG_ERROR("Compute用Null UAVの確保に失敗しました\n");
		Shutdown();
		return false;
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = DXGI_FORMAT_UNKNOWN;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
	uavDesc.Buffer.FirstElement = 0;
	uavDesc.Buffer.NumElements = 1;
	uavDesc.Buffer.StructureByteStride = sizeof(float);
	uavDesc.Buffer.CounterOffsetInBytes = 0;
	uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

	_device->CreateUnorderedAccessView(nullptr, nullptr, &uavDesc, nullUAV.cpu);

	device = _device;
	isInitialize = true;
	return true;
}

void ComputeSystem::Shutdown()
{
	DescriptorManager& descriptorManager{ DescriptorManager::Instance() };

	if (nullSRV.IsValid())
	{
		descriptorManager.Free(HeapType::CBV_SRV_UAV, nullSRV);
		nullSRV = {};
	}

	if (nullUAV.IsValid())
	{
		descriptorManager.Free(HeapType::CBV_SRV_UAV, nullUAV);
		nullSRV = {};
	}

	ringCBV.Shutdown();
	// 終了時は要求配列の確保済みメモリも返す
	std::vector<ComputeDispatchCommand>{}.swap(commands);
	device = nullptr;
	isInitialize = false;
}

bool ComputeSystem::RegisterDispatch(const ComputeDispatchDesc& _desc, const void* _data, std::size_t _parameterSize)
{

}

bool ComputeSystem::Flush(ID3D12GraphicsCommandList* _cmd)
{

}

void ComputeSystem::Reset()
{
	if (!isInitialize) return;
	commands.clear();
	ringCBV.Reset();
}
