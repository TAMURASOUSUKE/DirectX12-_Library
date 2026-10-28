#include <new>
#include <cstring>
#include <cstdint>
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
		nullUAV = {};
	}

	ringCBV.Shutdown();
	// 終了時は要求配列の確保済みメモリも返す
	std::vector<ComputeDispatchCommand>{}.swap(commands);
	device = nullptr;
	isInitialize = false;
}

bool ComputeSystem::RegisterDispatch(const ComputeDispatchDesc& _desc, const void* _data, std::size_t _parameterSize)
{
	if (!isInitialize)
	{
		DEBUG_LOG_ERROR("ComputeSystemが初期化されていません\n");
		return false;
	}

	// 確保した範囲を超えて登録しようとしない
	if (commands.size() >= MAX_COMPUTE_DISPATCH_COUNT)
	{
		DEBUG_LOG_ERROR("Computeの実行予約登録上限に達しました\n");
		return false;
	}

	// Handleが壊れていないか確認
	if (!_desc.pipeline.IsValid())
	{
		DEBUG_LOG_ERROR("ComputePipelineが未設定です\n");
		return false;
	}

	// このAPIでは何もしないグループ数0を許さない
	const auto checkGroupCount
	{
		[](const std::uint32_t _count)
		{
			return _count != 0 && _count <= D3D12_CS_DISPATCH_MAX_THREAD_GROUPS_PER_DIMENSION;
		}
	};

	if (!checkGroupCount(_desc.groupCountX) || !checkGroupCount(_desc.groupCountY) || !checkGroupCount(_desc.groupCountZ))
	{
		DEBUG_LOG_ERROR("Computeのスレッドグループ数が不正です\n");
		return false;
	}

	//  コピーするデータがある場合はポインタが有効化チェックする
	if (_parameterSize > MAX_COMPUTE_PARAMETER_SIZE || (_parameterSize > 0 && !_data))
	{
		DEBUG_LOG_ERROR("データが不正です\n");
		return false;
	}

	// 指定された種類とハンドルの形式が正しいかチェックする
	const auto isBindingWellFormed
	{
		[](const ComputeResourceBinding& _binding)
		{
			switch (_binding.type)
			{
			case ComputeResourceType::None:
				return !_binding.texture.IsValid() && !_binding.buffer.IsValid();
			case ComputeResourceType::Buffer:
				return !_binding.texture.IsValid() && _binding.buffer.IsValid();
			case ComputeResourceType::Texture:
				return _binding.texture.IsValid() && !_binding.buffer.IsValid();
			default:
				DEBUG_LOG_ERROR("不正な種類が指定されています\n");
				return false;
			}
		}
	};

	// 入力と出力それぞれ全てチェックする
	for (std::size_t i = 0; i < _desc.inputs.size(); i++)
	{
		if (!isBindingWellFormed(_desc.inputs[i]))
		{
			DEBUG_LOG_ERROR("Computeの入力指定が不正です Slot = {}", i);
			return false;
		}
	}

	for (std::size_t i = 0; i < _desc.outputs.size(); i++)
	{
		if (!isBindingWellFormed(_desc.outputs[i]))
		{
			DEBUG_LOG_ERROR("Computeの出力指定が不正です Slot = {}", i);
			return false;
		}
	}

	// 要求を配列内へ直接作りデータをコピーする
	ComputeDispatchCommand& command{ commands.emplace_back() };
	command.desc = _desc;
	command.parameterSize = _parameterSize;
	// 指定された部分だけ上書きする
	if (_parameterSize > 0) std::memcpy(command.parameterData.data(), _data, _parameterSize);
	return true;
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
