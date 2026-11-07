#include <algorithm>
#include <cstring>
#include <cstddef>
#include "../Debug/DebugLogs.h"
#include "GraphicsDevice.h"
#include "GraphicsResourceManager.h"
#include "GraphicsMetrics.h"
#include "VFXMeshSystem.h"

bool VFXMeshSystem::Setup(ID3D12RootSignature* _rootSignature)
{
	if (isInitialized)
	{
		DEBUG_LOG_WARNING("VFXMeshSystemはすでに初期化されています\n");
		return false;
	}
	if (!_rootSignature)
	{
		DEBUG_LOG_ERROR("VFX用RootSignatureが無効です\n");
		return false;
	}

	// GPUが使用中のフレーム領域をCPUが上書きしないようにバックバッファ数だけ独立して確保する
	const UINT bufferSize{ static_cast<UINT>(MAX_VFX_VERTEX_COUNT * sizeof(VFXVertex)) };
	for (DynamicBuffer& buffer : vertexBuffers)
	{
		buffer = GraphicsResourceManager::Instance().CreateDynamicBuffer(bufferSize);
		if (!buffer.resource || !buffer.mappedPtr)
		{
			DEBUG_LOG_ERROR("VFX頂点領域の作成に失敗しました\n");
			Shutdown();
			return false;
		}
	}

	// VP行列は1フレームに1回だけ更新する
	if (!frameRingCBV.Setup(static_cast<UINT>(sizeof(VFXFrameCB)), 1))
	{
		DEBUG_LOG_ERROR("VFXFrameCB用RingCBVの作成に失敗しました\n");
		Shutdown();
		return false;
	}

	// 描画1回につきスロット4つ + 未設定用のゼロ番地1つ
	const UINT parameterUpdatesPerFrame{ static_cast<UINT>(MAX_VFX_DRAW_COUNT * MATERIAL_PARAMETER_SLOT_COUNT + 1) };
	if (!userMaterialParameterRingCBV.Setup(static_cast<UINT>(MAX_MATERIAL_PARAMETER_SIZE), parameterUpdatesPerFrame))
	{
		DEBUG_LOG_ERROR("VFX用MaterialParameter RingCBVの作成に失敗しました\n");
		Shutdown();
		return false;
	}

	rootSignature = _rootSignature;
	drawPackets.reserve(MAX_VFX_DRAW_COUNT);
	isInitialized = true;
	return true;
}

void VFXMeshSystem::Shutdown()
{
	userMaterialParameterRingCBV.Shutdown();
	frameRingCBV.Shutdown();
	for (DynamicBuffer& buffer : vertexBuffers)
	{
		buffer = {};
	}
	drawPackets = std::vector<VFXDrawPacket>{}; // 終了時なのでcapacityも返却
	rootSignature = nullptr;
	zeroMaterialParameterAddress = 0;
	writtenVertexCount = 0;
	droppedVertexCount = 0;
	droppedPacketCount = 0;
	isInitialized = false;
}

void VFXMeshSystem::Reset()
{
	if (!isInitialized) return;

	// 超過通知
	if (droppedPacketCount > 0) DEBUG_LOG_WARNING("VFXメッシュの上限超過により{}件({}頂点)を破棄しました\n", droppedPacketCount, droppedVertexCount);
	droppedPacketCount = 0;
	droppedVertexCount = 0;

	writtenVertexCount = 0;
	drawPackets.clear();
	frameRingCBV.Reset();
	userMaterialParameterRingCBV.Reset();

	// 未設定スロットに結びつけるゼロ埋めの番地(ModelRendererと同じ方式)
	const std::array<std::byte, MAX_MATERIAL_PARAMETER_SIZE> zeroParameter{};
	zeroMaterialParameterAddress = userMaterialParameterRingCBV.Update(zeroParameter.data(), static_cast<UINT>(zeroParameter.size()));
	if (zeroMaterialParameterAddress == 0) DEBUG_LOG_ERROR("VFX用ゼロパラメータのCBV更新に失敗しました\n");
}

bool VFXMeshSystem::Register(std::span<const VFXVertex> _vertices, MaterialHandle _material)
{
	if (!isInitialized) return false;

	const std::size_t vertexCount{ _vertices.size() };

	// STRIPで三角形が1枚もできないまたは頂点数・描画数の上限を超えるなら捨てる
	if (vertexCount < 3 || writtenVertexCount + vertexCount > MAX_VFX_VERTEX_COUNT || drawPackets.size() >= MAX_VFX_DRAW_COUNT)
	{
		droppedVertexCount += vertexCount;
		droppedPacketCount++;
		return false;
	}

	if (!_material.IsValid())
	{
		DEBUG_LOG_ERROR("VFXメッシュに無効なMaterialHandleが渡されました\n");
		return false;
	}

	// 並べ替えに使う位置(全頂点の位置の平均)
	Vector3 centroid{ Vector3::Zero };
	for (const VFXVertex& vertex : _vertices)
	{
		centroid += Vector3{ vertex.position[0], vertex.position[1], vertex.position[2] };
	}
	centroid /= static_cast<float>(vertexCount);

	// 現在フレームのMap済み領域の書き込み位置へ直接写す
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	DynamicBuffer& buffer{ vertexBuffers[frameIndex] };
	if (!buffer.mappedPtr)
	{
		DEBUG_LOG_ERROR("VFX頂点領域が無効です\n");
		return false;
	}

	VFXVertex* destination{ static_cast<VFXVertex*>(buffer.mappedPtr) + writtenVertexCount }; // VFXVertex*への加算は頂点数単位で進む
	std::memcpy(destination, _vertices.data(), vertexCount * sizeof(VFXVertex));

	// 描画単位として記録する
	VFXDrawPacket packet{};
	packet.vertexStart = writtenVertexCount;
	packet.vertexCount = static_cast<UINT>(vertexCount);
	packet.sortPosition = centroid;
	packet.material = _material;
	packet.instanceCount = 1;
	drawPackets.push_back(packet);

	writtenVertexCount += static_cast<UINT>(vertexCount); // 次の書き込み位置へ進める
	return true;
}

bool VFXMeshSystem::RegisterInstanced(MaterialHandle _material, UINT _instanceCount, const Vector3& _sortPosition)
{
	if (!isInitialized) return false;

	if (_instanceCount == 0 || drawPackets.size() >= MAX_VFX_DRAW_COUNT)
	{
		droppedPacketCount++;
		return false;
	}

	if (!_material.IsValid())
	{
		DEBUG_LOG_ERROR("VFX粒子に無効なMaterialHandleが渡されました\n");
		return false;
	}

	// 描画単位として記録する
	VFXDrawPacket packet{};
	packet.vfxDrawKind = VFXDrawKind::Instance;
	packet.instanceCount = _instanceCount;
	packet.sortPosition = _sortPosition;
	packet.material = _material;
	packet.vertexStart = 0;
	packet.vertexCount = 4;
	drawPackets.push_back(packet);
	return true;
}

bool VFXMeshSystem::Flush(const Mat4x4& _viewProjection, const Vector3& _cameraPosition, const Quaternion& _cameraRotation)
{
	if (!isInitialized) return false;
	if (drawPackets.empty()) return true; // 描画対象がなくても異常ではない

	ID3D12GraphicsCommandList* cmd{ GraphicsDevice::Instance().GetCommandList() };
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	const DynamicBuffer& vertexBuffer{ vertexBuffers[frameIndex] };
	if (!cmd || !rootSignature || !vertexBuffer.resource || zeroMaterialParameterAddress == 0)
	{
		DEBUG_LOG_ERROR("VFX描画に必要な状態が無効です\n");
		return false;
	}

	// VP行列を1回だけGPUへ送る
	VFXFrameCB frameData{};
	const Vector3 cameraRight{ _cameraRotation.RotateVector(Vector3::Right) };
	const Vector3 cameraUp{ _cameraRotation.RotateVector(Vector3::Up) };
	frameData.viewProjection = _viewProjection;
	frameData.cameraRight[0] = cameraRight.x;
	frameData.cameraRight[1] = cameraRight.y;
	frameData.cameraRight[2] = cameraRight.z;
	frameData.cameraRight[3] = 0.0f;
	frameData.cameraUp[0] = cameraUp.x;
	frameData.cameraUp[1] = cameraUp.y;
	frameData.cameraUp[2] = cameraUp.z;
	frameData.cameraUp[3] = 0.0f;
	const D3D12_GPU_VIRTUAL_ADDRESS frameAddress{ frameRingCBV.Update(&frameData, static_cast<UINT>(sizeof(frameData))) };
	if (frameAddress == 0)
	{
		DEBUG_LOG_ERROR("VFXFrameCBのGPU転送に失敗しました\n");
		return false;
	}

	// 半透明は奥から手前に描くのでカメラから遠い順に並べ替える
	for (VFXDrawPacket& packet : drawPackets)
	{
		packet.sortDepth = Vector3::DistanceSquared(_cameraPosition, packet.sortPosition);
	}
	std::sort(drawPackets.begin(), drawPackets.end(), [](const VFXDrawPacket& _left, const VFXDrawPacket& _right) { return _left.sortDepth > _right.sortDepth; });

	// 全メッシュで共通の状態
	cmd->SetGraphicsRootSignature(rootSignature);
	cmd->SetGraphicsRootConstantBufferView(0, frameAddress); // RootParameter[0] = b0 VFXFrameCB
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

	const D3D12_GPU_VIRTUAL_ADDRESS vertexBase{ vertexBuffer.resource->GetGPUVirtualAddress() };
	GraphicsResourceManager& resourceManager{ GraphicsResourceManager::Instance() };
	bool succeeded{ true };

	for (const VFXDrawPacket& packet : drawPackets)
	{
		// 登録はユーザーコードの途中で起きるので、台帳が変わらないこの時点でLookupして検証する
		const MaterialData* material{ resourceManager.Lookup(packet.material) };
		if (!material)
		{
			DEBUG_LOG_ERROR("VFXメッシュのMaterialのLookupに失敗しました\n");
			succeeded = false;
			continue;
		}
		if (material->usage != ShaderUsage::VFX || material->pipelineSource != MaterialPipelineSource::Custom || !material->pipelineState)
		{
			DEBUG_LOG_ERROR("VFXメッシュのMaterialのUsageまたはPSOが不正です\n");
			succeeded = false;
			continue;
		}
		cmd->SetPipelineState(material->pipelineState.Get());

		// SetMaterialParameterの値をスロットごとに配達する(未設定はゼロ番地)
		for (std::size_t i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
		{
			D3D12_GPU_VIRTUAL_ADDRESS parameterAddress{ zeroMaterialParameterAddress };
			const MaterialParameterBlock& block{ material->parameters[i] };
			if (block.hasParameter)
			{
				parameterAddress = userMaterialParameterRingCBV.Update(block.parameterData.data(), static_cast<UINT>(block.parameterSize));
				if (parameterAddress == 0)
				{
					DEBUG_LOG_ERROR("VFX用MaterialParameterのGPU転送に失敗しました\n");
					parameterAddress = zeroMaterialParameterAddress;
				}
			}
			cmd->SetGraphicsRootConstantBufferView(MATERIAL_PARAMETER_ROOT_PARAM_BASE + static_cast<UINT>(i), parameterAddress);
		}

		// SetMaterialTextureのスロットごとに結びつける(空ならnullSRV、Lookup失敗ならエラーテクスチャ)
		for (std::size_t i = 0; i < MATERIAL_RESOURCE_PARAMETER_SLOT_COUNT; i++)
		{
			const D3D12_GPU_DESCRIPTOR_HANDLE texture{ resourceManager.ResolveMaterialResourceSRV(material->resourceSlots[i]) };
			cmd->SetGraphicsRootDescriptorTable(MATERIAL_TEXTURE_ROOT_PARAM_BASE + static_cast<UINT>(i), texture);
		}

		// このメッシュの区間だけを頂点バッファとして見せる(Descriptorではなく番地と大きさを直接渡す)
		D3D12_VERTEX_BUFFER_VIEW view{};
		view.BufferLocation = vertexBase + static_cast<UINT64>(packet.vertexStart) * VERTEX_STRIDE;
		view.SizeInBytes = packet.vertexCount * VERTEX_STRIDE;
		view.StrideInBytes = VERTEX_STRIDE;
		cmd->IASetVertexBuffers(0, 1, &view);
		cmd->DrawInstanced(packet.vertexCount, packet.instanceCount, 0, 0);
		GraphicsMetrics::RecordDrawCall(GraphicsPass::VFXMesh); // 描画回数の計測
	}

	return succeeded;
}
