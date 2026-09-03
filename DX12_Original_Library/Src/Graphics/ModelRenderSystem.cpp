#include <type_traits>
#include <algorithm>
#include "GraphicsResourceManager.h"
#include "../Debug/DebugLogs.h"
#include "GraphicsConstant.h"
#include "ModelRenderSystem.h"

namespace
{
	// 渡されたmaterialからパイプラインIDを返すヘルパー
	PipelineID SelectModelPipeline(const Material& _material)
	{
		const bool isBlend{ _material.alphaMode == MaterialAlphaMode::Blend };
		// Blendなら両面か判断して返す
		if (isBlend) return _material.doubleSided ? PipelineID::ModelBlendDoubleSided : PipelineID::ModelBlend;
		// Blendでは無い場合も同様に両面か判断する
		return _material.doubleSided ? PipelineID::ModelDoubleSided : PipelineID::Model;
	}
}

bool ModelRenderSystem::Setup(ShaderSystem* _shaderSystem, CameraSystem* _cameraSystem, LightSystem* _lightSystem, AnimationSystem* _animationSystem)
{
	if (!_shaderSystem || !_cameraSystem || !_lightSystem || !_animationSystem) return false;
	if (!renderer.Setup(_shaderSystem, _cameraSystem, _lightSystem)) return false;
	renderQueue.Setup();
	opaqueModels.reserve(MAX_MODEL_DRAW_PACKET_COUNT);
	blendModels.reserve(MAX_MODEL_DRAW_PACKET_COUNT);
	animationSystem = _animationSystem;
	cameraSystem = _cameraSystem;
	return true;
}

void ModelRenderSystem::Shutdown()
{
	opaqueModels = {};
	blendModels = {};
	renderer.Shutdown();
	renderQueue.Shutdown();
	animationSystem = nullptr;
	cameraSystem = nullptr;
	droppedPacketCount = 0;
}

void ModelRenderSystem::BeginFrame()
{
	// 超過通知
	if (droppedPacketCount > 0) DEBUG_LOG_WARNING("ModelDrawPacketの上限超過により{}件を破棄しました\n", droppedPacketCount);
	droppedPacketCount = 0;
	opaqueModels.clear();
	blendModels.clear();
	renderQueue.Reset(); // 保存依頼をフレームの先頭でリセット
	renderer.BeginFrame(); // 各CBVのリセットなど
}

void ModelRenderSystem::BuildDrawPackets()
{
	for (const ModelRenderCommand& renderCommand : renderQueue.GetCommands())
	{
		// visitを使って静的モデルかスキニングモデルかを分けて描画命令を行う
		std::visit(
			[this](const auto& _command)
			{
				// decltypeで型を取得し、decay_tでconstと参照を外す
				using CommandType = std::decay_t<decltype(_command)>;

				// constexprを使うことでコンパイル時に分岐させる
				if constexpr (std::is_same_v<CommandType, StaticModelRenderCommand>)
				{
					ModelData* model{ GraphicsResourceManager::Instance().Lookup(_command.modelHandle) };
					if (!model) return;
					if (!AppendDrawPackets(*model, nullptr, _command.transform)) return;
				}
				else if constexpr (std::is_same_v<CommandType, SkinningModelRenderCommand>)
				{
					AnimInstanceData* instance{ animationSystem->Lookup(_command.animInstanceHandle) };
					if (!instance) return;
					ModelData* model{ GraphicsResourceManager::Instance().Lookup(instance->modelHandle) };
					if (!model) return;
					if (!AppendDrawPackets(*model, instance, _command.transform)) return;
				}
			},
			renderCommand
		);
	}
	// 並べ替えを行う
	std::sort(opaqueModels.begin(), opaqueModels.end(), [](const ModelDrawPacket& _left, const ModelDrawPacket& _right) { return _left.sortDepth < _right.sortDepth; }); // 不透明は手前から奥に
	std::sort(blendModels.begin(), blendModels.end(), [](const ModelDrawPacket& _left, const ModelDrawPacket& _right) { return _left.sortDepth > _right.sortDepth; }); // 半透明は奥から手前

}

void ModelRenderSystem::FlushOpaque()
{
	if (opaqueModels.empty()) return;
	if (!renderer.BeginModelDraw()) return; // Model用共通データを設定する(RootSigやCBV,Topology)
	for (const ModelDrawPacket& packet : opaqueModels)
	{
		renderer.DrawSubMesh(packet);
	}
}

void ModelRenderSystem::FlushBlend()
{
	if (blendModels.empty()) return;
	// Primtive3Dの後に描画するのでGPU状態が変更されていることを考慮したてBlend描画前にModelの状態をもう一度設定
	if (!renderer.BeginModelDraw()) return; // Model用共通データを設定する(RootSigやCBV,Topology)
	for (const ModelDrawPacket& packet : blendModels)
	{
		renderer.DrawSubMesh(packet);
	}
}

bool ModelRenderSystem::Register(ModelHandle _model, const Transform& _transform)
{
	return renderQueue.Register(_model, _transform);
}

bool ModelRenderSystem::Register(AnimInstanceHandle _animInstance, const Transform& _transform)
{
	return renderQueue.Register(_animInstance, _transform);
}

bool ModelRenderSystem::AppendDrawPackets(const ModelData& _model, const AnimInstanceData* _animation, const Transform& _transform)
{
	const std::size_t currentCount{ opaqueModels.size() + blendModels.size() }; // 現在の描画が行われる数
	if (currentCount > MAX_MODEL_DRAW_PACKET_COUNT)
	{
		DEBUG_LOG_ERROR("ModelDrawPacket数が上限を超えた不正な状態です\n");
		return false;
	}
	// 残っている描画命令数
	const std::size_t remainingCount{ MAX_MODEL_DRAW_PACKET_COUNT - currentCount };
	if (_model.subMeshes.size() > remainingCount)
	{
		// モデルのサブメッシュの数が残りの可能数を超えているなら超過記録変数に入れる
		droppedPacketCount += _model.subMeshes.size();
		return false;
	}

	// モデルのアドレス等を準備する
	PreparedModelDrawData preparedData{};
	if (!renderer.PrepareModelData(_transform, _animation, preparedData)) return false;

	const float sortDepth{ Vector3::DistanceSquared(cameraSystem->GetCameraPosition(), _transform.GetPosition())}; // モデルからカメラへの距離の二乗
	for (const SubMesh& subMesh : _model.subMeshes)
	{
		ModelDrawPacket packet{ &subMesh, SelectModelPipeline(subMesh.material), sortDepth, preparedData }; // GPUへ送る描画単位の作成

		// alphaModeで比較してvectorへのpushを選択する
		if (subMesh.material.alphaMode == MaterialAlphaMode::Blend) blendModels.push_back(packet);
		else opaqueModels.push_back(packet);
	}
	return true;
}
