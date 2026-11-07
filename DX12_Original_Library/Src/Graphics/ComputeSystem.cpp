#include <new>
#include <cstring>
#include <cstdint>
#include <d3d12.h>
#include "../External/Common/d3dx12.h"
#include "GraphicsResourceManager.h"
#include "DescriptorManager.h"
#include "../Debug/DebugLogs.h"
#include "ComputeSystem.h"

namespace
{
	// 現在の状態が今回要求する状態を満たしているか
	bool IsComputeSatisfied(D3D12_RESOURCE_STATES _currentState, D3D12_RESOURCE_STATES _requiredState)
	{
		if (_currentState == _requiredState) return true;
		// Computeで読むために必要な状態をbit演算で含んでいるかを確認してTransitionが不要かを確かめる
		return _requiredState != D3D12_RESOURCE_STATE_COMMON && (_currentState & _requiredState) == _requiredState;
	}

	// Resourceと状態変数への参照を登録する すでに登録済みのResourceなら矛盾を確認して省略する
	bool AddComputeBarrierResource(ComputeBarrierBatch& _batch, ID3D12Resource* _resource, D3D12_RESOURCE_STATES& _currentState, D3D12_RESOURCE_STATES _requiredState, bool _requireUAVOrdering = false)
	{
		if (!_resource)
		{
			DEBUG_LOG_ERROR("Barrierの対象Resourceが不正です\n");
			return false;
		}

		if (_batch.resourceCount > _batch.resources.size())
		{
			DEBUG_LOG_ERROR("BarrierのResource数が不正です\n");
			return false;
		}

		// 同じResourceを複数のスロットで使っても登録を重複させない
		for (std::size_t i = 0; i < _batch.resourceCount; i++)
		{
			ComputeBarrierResource& entry{ _batch.resources[i] };
			if (entry.resource != _resource) continue;

			// 同じResourceの状態を別々の変数で管理しない
			if (entry.currentState != &_currentState)
			{
				DEBUG_LOG_ERROR("同じResourceの状態管理が一致していません\n");
				return false;
			}

			// 同じResourceのSRV入力とUAV出力の同時指定を許可しない
			if (entry.requiredState != _requiredState)
			{
				DEBUG_LOG_ERROR("同じResourceへ矛盾する状態が指定されました\n");
				return false;
			}

			// どちらかの指定で順序保証が必要ならそれを維持する
			entry.requireUAVOrdering = entry.requireUAVOrdering || _requireUAVOrdering;

			return true;
		}

		if (_batch.resourceCount >= _batch.resources.size())
		{
			DEBUG_LOG_ERROR("Compute用BarrierのResource登録上限を超えました\n");
			return false;
		}

		ComputeBarrierResource& entry{ _batch.resources[_batch.resourceCount] };
		entry.resource = _resource;
		entry.currentState = &_currentState; // 元の変数の場所を保存する
		entry.requiredState = _requiredState;
		entry.requireUAVOrdering = _requireUAVOrdering;

		_batch.resourceCount++;
		return true;
	}

	// 必要なBarrierを一括登録してその後で元のcurrentStateを更新する
	bool RecordComputeBarriers(ID3D12GraphicsCommandList* _cmd, ComputeBarrierBatch& _batch)
	{
		if (!_cmd || _batch.resources.size() < _batch.resourceCount)
		{
			DEBUG_LOG_ERROR("Compute用Barrierの記録引数が不正です\n");
			return false;
		}

		// 参照先が設定されているか確認する
		for (std::size_t i = 0; i < _batch.resourceCount; i++)
		{
			const ComputeBarrierResource& entry{ _batch.resources[i] };

			if (!entry.resource || !entry.currentState)
			{
				DEBUG_LOG_ERROR("BarrierのResourceか状態参照が不正です\n");
				return false;
			}
		}

		UINT barrierCount{ 0 }; // 必要になったBarrierの個数
		for (std::size_t i = 0; i < _batch.resourceCount; i++)
		{
			const ComputeBarrierResource& entry{ _batch.resources[i] };
			const D3D12_RESOURCE_STATES before{ *entry.currentState };

			if (!IsComputeSatisfied(before, entry.requiredState))
			{
				D3D12_RESOURCE_BARRIER& barrier{ _batch.barriers[barrierCount] };
				barrier = {}; // 念のため初期化

				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION; // 要求の状態を満たしていないので遷移させる
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

				barrier.Transition.pResource = entry.resource;
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

				barrier.Transition.StateBefore = before;
				barrier.Transition.StateAfter = entry.requiredState;

				barrierCount++;
			}
			else if (entry.requiredState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && entry.requireUAVOrdering)
			{
				// UAVを継続する場合は必要に応じてアクセス順序を保証する
				D3D12_RESOURCE_BARRIER& barrier{ _batch.barriers[barrierCount] };
				barrier = {};

				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.UAV.pResource = entry.resource;

				barrierCount++;
			}
		}

		// 一つのResourceにつき最大一つなので配列の容量を超えない
		if (barrierCount > 0) _cmd->ResourceBarrier(barrierCount, _batch.barriers.data());

		// CommandListへ記録した後 元の状態変数を更新
		for (std::size_t i = 0; i < _batch.resourceCount; i++)
		{
			const ComputeBarrierResource& entry{ _batch.resources[i] };
			if (!IsComputeSatisfied(*entry.currentState, entry.requiredState)) *entry.currentState = entry.requiredState;
		}

		// 配列を再確保せず次のDispatchで再利用できるようにする
		_batch.resourceCount = 0;
		return true;
	}
}

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
		DEBUG_LOG_ERROR("Computeの実行要求メモリ確保に失敗しました\n");
		Shutdown();
		return false;
	}

	try
	{
		transitionAllResourceBarriers.reserve(MAX_COMPUTE_DISPATCH_COUNT);
	}
	catch (std::bad_alloc&)
	{
		DEBUG_LOG_ERROR("ComputeのTexture遷移Barreir配列メモリ確保に失敗しました\n");
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
	if (!isInitialize)
	{
		DEBUG_LOG_ERROR("ComputeSystemが初期化されていません\n");
		return false;
	}

	if (!_cmd)
	{
		DEBUG_LOG_ERROR("CommandListが不正です\n");
		return false;
	}

	if (isFlush)
	{
		DEBUG_LOG_ERROR("このフレームではすでにFlushが実行されています\n");
		return false;
	}

	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };

	// 全ての要求をチェックして安全かを見る
	for (const auto& command : commands)
	{
		ComputeBarrierBatch barrierBatch{}; // まとめてバリアを使うために行う
		ComputePipelineData* pipelineData{ resourceManager.Lookup(command.desc.pipeline) };
		if (!pipelineData)
		{
			DEBUG_LOG_ERROR("PipelineDataが不正です\n");
			return false;
		}

		//  コピーするデータがある場合はポインタが有効化チェックする
		if (command.parameterSize > MAX_COMPUTE_PARAMETER_SIZE)
		{
			DEBUG_LOG_ERROR("データが不正です\n");
			return false;
		}

		// 入力と出力それぞれ全てチェックする
		for (std::size_t i = 0; i < MAX_COMPUTE_SRV_COUNT; i++)
		{
			const ComputeResourceBinding& binding{ command.desc.inputs[i] };
			switch (binding.type)
			{
			case ComputeResourceType::None:
				continue;
			case ComputeResourceType::Buffer:
			{
				if (!binding.buffer.IsValid())
				{
					DEBUG_LOG_ERROR("入力されたBufferのハンドルが不正です\n");
					return false;
				}
				GPUBufferData* data{ resourceManager.Lookup(binding.buffer) };
				if (!data)
				{
					DEBUG_LOG_ERROR("入力されたBufferが不正です\n");
					return false;
				}
				if (!data->srvHandle.IsValid())
				{
					DEBUG_LOG_ERROR("入力のViewが不正です\n");
					return false;
				}
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
				{
					DEBUG_LOG_ERROR("入力用Bufferの登録に失敗しました\n");
					return false;
				}
				break;
			}
			case ComputeResourceType::Texture:
			{
				if (!binding.texture.IsValid())
				{
					DEBUG_LOG_ERROR("入力されたTextureのハンドルが不正です\n");
					return false;
				}
				TextureData* data{ resourceManager.Lookup(binding.texture) };
				if (!data)
				{
					DEBUG_LOG_ERROR("入力されたTextureが不正です\n");
					return false;
				}
				if (!data->srvHandle.IsValid())
				{
					DEBUG_LOG_ERROR("入力のViewが不正です\n");
					return false;
				}
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE))
				{
					DEBUG_LOG_ERROR("入力用Textureの登録に失敗しました\n");
					return false;
				}
				break;
			}
			}
		}

		for (std::size_t i = 0; i < MAX_COMPUTE_UAV_COUNT; i++)
		{
			const ComputeResourceBinding& binding{ command.desc.outputs[i] };
			switch (binding.type)
			{
			case ComputeResourceType::None:
				continue;
			case ComputeResourceType::Buffer:
			{
				if (!binding.buffer.IsValid())
				{
					DEBUG_LOG_ERROR("出力するBufferのハンドルが不正です\n");
					return false;
				}
				GPUBufferData* data{ resourceManager.Lookup(binding.buffer) };
				if (!data)
				{
					DEBUG_LOG_ERROR("出力するBufferが不正です\n");
					return false;
				}
				if (!data->uavHandle.IsValid())
				{
					DEBUG_LOG_ERROR("出力のViewが不正です\n");
					return false;
				}
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true))
				{
					DEBUG_LOG_ERROR("出力用Bufferの登録に失敗しました\n");
					return false;
				}
				break;
			}
			case ComputeResourceType::Texture:
			{
				if (!binding.texture.IsValid())
				{
					DEBUG_LOG_ERROR("出力されるTextureのハンドルが不正です\n");
					return false;
				}
				TextureData* data{ resourceManager.Lookup(binding.texture) };
				if (!data)
				{
					DEBUG_LOG_ERROR("出力されるTextureが不正です\n");
					return false;
				}
				if (!data->uavHandle.IsValid())
				{
					DEBUG_LOG_ERROR("出力のViewが不正です\n");
					return false;
				}
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true))
				{
					DEBUG_LOG_ERROR("出力用Textureの登録に失敗しました\n");
					return false;
				}
				break;
			}
			}
		}

	}

	isFlush = true; // 検証に成功したのでここでtrue

	// 全てチェックしたのでここから記録を行う
	DescriptorManager::Instance().SetDiscriptor(_cmd);
	for (const auto& command : commands)
	{
		ComputeBarrierBatch barrierBatch{}; // まとめてバリアを使うために行う
		const D3D12_GPU_VIRTUAL_ADDRESS gpuVirtualAddress{ ringCBV.Update(command.parameterData.data(), static_cast<UINT>(command.parameterSize)) };
		if (gpuVirtualAddress == 0)
		{
			DEBUG_LOG_ERROR("GPU仮想アドレス取得に失敗しました\n");
			return false;
		}
		ComputePipelineData* pipelineData{ resourceManager.Lookup(command.desc.pipeline) };
		_cmd->SetComputeRootSignature(pipelineData->rootSignature.Get());
		_cmd->SetPipelineState(pipelineData->pipelineState.Get());
		_cmd->SetComputeRootConstantBufferView(static_cast<UINT>(ComputeRootParameter::ConstantBuffer), gpuVirtualAddress);

		// 入力
		for (std::size_t i = 0; i < MAX_COMPUTE_SRV_COUNT; i++)
		{
			const ComputeResourceBinding& binding{ command.desc.inputs[i] };
			D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle{};
			switch (binding.type)
			{
			case ComputeResourceType::None:
				descriptorHandle = nullSRV.gpu; // 未設定を明示する
				break;
			case ComputeResourceType::Buffer:
			{
				GPUBufferData* data{ resourceManager.Lookup(binding.buffer) };
				descriptorHandle = data->srvHandle.gpu;
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) return false;
				break;
			}
			case ComputeResourceType::Texture:
			{
				TextureData* data{ resourceManager.Lookup(binding.texture) };
				descriptorHandle = data->srvHandle.gpu;
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE)) return false;
				break;
			}
			}

			_cmd->SetComputeRootDescriptorTable(static_cast<UINT>(ComputeRootParameter::InputBuffer) + static_cast<UINT>(i), descriptorHandle);
		}

		// 出力
		for (std::size_t i = 0; i < MAX_COMPUTE_UAV_COUNT; i++)
		{
			const ComputeResourceBinding& binding{ command.desc.outputs[i] };
			D3D12_GPU_DESCRIPTOR_HANDLE descriptorHandle{};
			switch (binding.type)
			{
			case ComputeResourceType::None:
				descriptorHandle = nullUAV.gpu;
				break;
			case ComputeResourceType::Buffer:
			{
				GPUBufferData* data{ resourceManager.Lookup(binding.buffer) };
				descriptorHandle = data->uavHandle.gpu;
				// UAV出力は書き込み依存があるものとする
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true)) return false;
				break;
			}
			case ComputeResourceType::Texture:
			{
				TextureData* data{ resourceManager.Lookup(binding.texture) };
				descriptorHandle = data->uavHandle.gpu;
				if (!AddComputeBarrierResource(barrierBatch, data->resource.Get(), data->currentState, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true)) return false;
				break;
			}
			}
			_cmd->SetComputeRootDescriptorTable(static_cast<UINT>(ComputeRootParameter::OutputBuffer) + static_cast<UINT>(i), descriptorHandle);
		}

		// 命令記録
		if (!RecordComputeBarriers(_cmd, barrierBatch)) return false;
		_cmd->Dispatch(command.desc.groupCountX, command.desc.groupCountY, command.desc.groupCountZ);
	}

	transitionAllResourceBarriers.clear(); // あらかじめクリアしておく

	// Buffer若しくはTextureのリソースとStateをみてバリアに積む
	const auto& transitionToAllShaderResource
	{
		[this, &resourceManager](const ComputeResourceBinding& _binding)
		{
			ID3D12Resource* resource{nullptr};
			D3D12_RESOURCE_STATES* currentState{ nullptr };
			switch (_binding.type)
			{
			case ComputeResourceType::Buffer:
			{
				GPUBufferData* data{ resourceManager.Lookup(_binding.buffer) };
				if (!data)
				{
					DEBUG_LOG_ERROR("GPUBufferのLookupに失敗しました\n");
					return false;
				}
				resource = data->resource.Get();
				currentState = &data->currentState;
			}
				break;
			case ComputeResourceType::Texture:
			{
				TextureData* data{ resourceManager.Lookup(_binding.texture) };
				if (!data)
				{
					DEBUG_LOG_ERROR("TextureのLookupに失敗しました\n");
					return false;
				}
				resource = data->resource.Get();
				currentState = &data->currentState;
			}
				break;
			case ComputeResourceType::None:
				break;
			default:
				DEBUG_LOG_ERROR("不正なtypeが指定されています\n");
				return false;
			}

			if (!resource) return true;

			// 現在の状態がALL_SHADER_RESOURCEを含んでいれば弾く
			if (IsComputeSatisfied(*currentState, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE)) return true;
			// Barrierの作成
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(resource, *currentState, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) };
			transitionAllResourceBarriers.push_back(barrier);
			// 追加したので現在の状態を更新
			*currentState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

			return true;
		}
	};

	// TextureとBufferを明示的にTransitionする
	for (const auto& command : commands)
	{
		// 入力
		for (std::size_t i = 0; i < MAX_COMPUTE_SRV_COUNT; i++)
		{
			const ComputeResourceBinding& binding{ command.desc.inputs[i] };
			if (!transitionToAllShaderResource(binding)) DEBUG_LOG_WARNING("渡されたリソースをbarrierに積めません\n");
		}

		for (std::size_t i = 0; i < MAX_COMPUTE_UAV_COUNT; i++)
		{
			const ComputeResourceBinding& binding{ command.desc.outputs[i] };
			if (!transitionToAllShaderResource(binding)) DEBUG_LOG_WARNING("渡されたリソースをbarrierに積めません\n");
		}
	}
	// 終了前に遷移をする
	if(!transitionAllResourceBarriers.empty()) _cmd->ResourceBarrier(static_cast<UINT>(transitionAllResourceBarriers.size()), transitionAllResourceBarriers.data());
	return true;
}

void ComputeSystem::Reset()
{
	if (!isInitialize) return;
	commands.clear();
	ringCBV.Reset();
	isFlush = false; 
}
