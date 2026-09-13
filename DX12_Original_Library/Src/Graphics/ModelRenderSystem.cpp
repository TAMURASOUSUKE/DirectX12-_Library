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

bool ModelRenderSystem::FlushShadow(D3D12_GPU_VIRTUAL_ADDRESS _shadowFrameAddress)
{
	if (_shadowFrameAddress == 0)
	{
		DEBUG_LOG_ERROR("Shadow描画用の光源行列GPUアドレスが不正です\n");
		return false;
	}

	// 描画対象がなくても異常ではない
	if (opaqueModels.empty()) return true;

	// RootSignature・Shadow用PSO・光源VPを設定する
	if (!renderer.BeginShadowDraw(_shadowFrameAddress)) return false;

	bool succeeded{ true };

	for (const ModelDrawPacket& packet : opaqueModels)
	{
		// 1つ失敗しても描画可能な残りのPacketは処理する
		if (!renderer.DrawShadowSubMesh(packet)) succeeded = false;
	}

	return succeeded;
}

void ModelRenderSystem::FlushOpaque(D3D12_GPU_VIRTUAL_ADDRESS _shadowFrameAddress, D3D12_GPU_DESCRIPTOR_HANDLE _shadowMapSRV)
{
	if (opaqueModels.empty()) return;
	if (!renderer.BeginModelDraw(_shadowFrameAddress, _shadowMapSRV)) return; // Model用共通データを設定する(RootSigやCBV,Topology)
	for (const ModelDrawPacket& packet : opaqueModels)
	{
		renderer.DrawSubMesh(packet);
	}
}

void ModelRenderSystem::FlushBlend(D3D12_GPU_VIRTUAL_ADDRESS _shadowFrameAddress, D3D12_GPU_DESCRIPTOR_HANDLE _shadowMapSRV)
{
	if (blendModels.empty()) return;
	// Primtive3Dの後に描画するのでGPU状態が変更されていることを考慮したてBlend描画前にModelの状態をもう一度設定
	if (!renderer.BeginModelDraw(_shadowFrameAddress, _shadowMapSRV)) return; // Model用共通データを設定する(RootSigやCBV,Topology)
	for (const ModelDrawPacket& packet : blendModels)
	{
		renderer.DrawSubMesh(packet);
	}
}

bool ModelRenderSystem::Register(ModelHandle _model, const Transform& _transform, MaterialHandle _drawMaterialOverride)
{
	return renderQueue.Register(_model, _transform, _drawMaterialOverride);
}

bool ModelRenderSystem::Register(AnimInstanceHandle _animInstance, const Transform& _transform, MaterialHandle _drawMaterialOverride)
{
	return renderQueue.Register(_animInstance, _transform, _drawMaterialOverride);
}

bool ModelRenderSystem::RegisterLOD(std::span<const ModelLODLevel> _levels, ModelLODState& _state, const Transform& _transform, float _hysteresisDistance)
{
	// LOD設定がなければ描画するモデルを選べないのでfalse
	if (_levels.empty())
	{
		DEBUG_LOG_ERROR("ModelLevelが一つも設定されていません\n");
		return false;
	}

	// 最初のLODは0mから使用できる必要がある
	if (_levels.front().minDistance != 0.0f) // frontで先頭参照を取ってくる
	{
		DEBUG_LOG_ERROR("最初のLODのMinDistanceは0.0fにしてください\n");
		return false;
	}

	float prevDistance{ -1.0f }; // 前の距離

	// Handleと距離設定を事前検証
	for (const ModelLODLevel& level : _levels)
	{
		if (!level.model.IsValid())
		{
			DEBUG_LOG_ERROR("ModelLODLevelに無効なModelHandleがあります\n");
			return false;
		}
		if (level.minDistance < 0.0f)
		{
			DEBUG_LOG_ERROR("LOD切り替え距離には0以上を指定してください\n");
			return false;
		}
		// 距離は近いLODから遠いLODの順である必要がある
		if (level.minDistance <= prevDistance)
		{
			DEBUG_LOG_ERROR("ModelLODLevelの距離が昇順ではありません\n");
			return false;
		}
		prevDistance = level.minDistance;
	}

	const float distanceSqueared{ Vector3::DistanceSquared(cameraSystem->GetCameraPosition(), _transform.GetPosition()) }; // モデル位置からカメラ距離の二乗
	
	// 初回またはLODの設定数の変更によって保存ずみIndexが範囲外になった場合はヒステリシスなしの現在距離参照にする
	if (!_state.isInitialized || _levels.size() <= _state.currentLevelIndex)
	{
		std::size_t selectedIndex{ 0 };
		// 初回は以前どのLODだったかが存在しないので通常のMindistanceを使う
		for (std::size_t index = 1; index < _levels.size(); index++)
		{
			const float threshold{ _levels[index].minDistance };
			const float thresholdSquared{ threshold * threshold };
			// 昇順なのでこれ以降のLODも条件を満たさない
			if (distanceSqueared < thresholdSquared) break;

			selectedIndex = index;
		}
		_state.currentLevelIndex = selectedIndex;
		_state.isInitialized = true;
	}
	else
	{
		// テレポート等に対応できるようにwhile(1フレームで複数の境界をまたいだ場合にはそのフレーム内で正しい位置に移動できるようにする)

		// カメラから遠ざかる方の判定
		while (_state.currentLevelIndex + 1 < _levels.size())
		{
			// 通常境界よりヒステリシス閾値分だけ遠く進むまで低詳細LODに切り替えない
			const std::size_t nextIndex{ _state.currentLevelIndex + 1 };
			const float switchDistance{ _levels[nextIndex].minDistance + _hysteresisDistance }; // 低詳細に設定されている距離 + ヒステリシス
			const float switchDistanceSquared{ switchDistance * switchDistance };

			if (distanceSqueared < switchDistanceSquared) break; // 昇順のため
			_state.currentLevelIndex++; // 現在のレベルを移動させる
		}

		// カメラに近づく方向の設定
		while (_state.currentLevelIndex > 0)
		{
			// 通常境界よりヒステリシス閾値分だけ近づくまで低詳細LODに切り替えない
			const float switchDistance{ (std::max)(_levels[_state.currentLevelIndex].minDistance - _hysteresisDistance, 0.0f) };
			const float switchDistanceSquared{ switchDistance * switchDistance };

			// 切り替え距離より遠ければ現在のLODを維持
			if (distanceSqueared >= switchDistanceSquared) break; // 昇順のため
			_state.currentLevelIndex--; // 現在のレベルを移動させる
		}
	}

	// 状態が選択しているモデルだけ既存のRenderQueueへ登録する
	return renderQueue.Register(_levels[_state.currentLevelIndex].model, _transform, MaterialHandle{}); // 現状はひとまず空のmaterialハンドルを渡す
}

bool ModelRenderSystem::AppendDrawPackets(const ModelData& _model, const AnimInstanceData* _animation, const Transform& _transform)
{
	// Boneを持つモデルにはBone数分のスキニング行列が必要なのでDrawModelだと単位行列を一つしか渡さないのでSkin付モデルを描画すると頂点が壊れる
	if (!_animation && !_model.bones.empty())
	{
		DEBUG_LOG_ERROR("SkinつきモデルはDrawModelでは描画できません CreateAnimInstaceとDrawAnimatedModelを使用してください\n");
		return false;
	}

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
