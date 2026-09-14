#include "../Debug/DebugLogs.h"
#include "GraphicsConstant.h"
#include "ModelRenderQueue.h"

void ModelRenderQueue::Setup()
{
	// 再初期化されても古い描画依頼を残さない
	commands.clear();
	droppedCommandCount = 0;

	// フレーム中のvector再確保を防ぐ
	commands.reserve(MAX_MODEL_RENDER_COMMAND_COUNT);
}

void ModelRenderQueue::Shutdown()
{
	// 要素と容量の開放
	commands = std::vector<ModelRenderCommand>{};
	droppedCommandCount = 0;
}

void ModelRenderQueue::Reset()
{
	if (droppedCommandCount > 0) DEBUG_LOG_WARNING("モデル描画依頼の登録上限を超えたため{}件を破棄しました\n", droppedCommandCount);

	// capacityは残して次フレームに利用する
	commands.clear();
	droppedCommandCount = 0;
}

bool ModelRenderQueue::Register(ModelHandle _model, const Transform& _transform, MaterialHandle _drawMaterialOverride)
{
	if (!_model.IsValid())
	{
		DEBUG_LOG_ERROR("モデル描画命令に無効なModelHandleが渡されました\n");
		return false;
	}
	if (commands.size() >= MAX_MODEL_RENDER_COMMAND_COUNT)
	{
		droppedCommandCount++;
		return false;
	}
	// Materialが外部から指定されていない場合は無効Handleなので、ここでは拒否しない

	// Transformは現在地のスナップショットとしてコピーする
	commands.emplace_back(StaticModelRenderCommand{ _model, _transform, _drawMaterialOverride });
	return true;
}

bool ModelRenderQueue::Register(AnimInstanceHandle _animInstance, const Transform& _transform, MaterialHandle _drawMaterialOverride)
{
	if (!_animInstance.IsValid())
	{
		DEBUG_LOG_ERROR("モデル描画命令に無効なAnimInstanceHandleが渡されました\n");
		return false;
	}
	if (commands.size() >= MAX_MODEL_RENDER_COMMAND_COUNT)
	{
		droppedCommandCount++;
		return false;
	}
	// Materialが外部から指定されていない場合は無効Handleなので、ここでは拒否しない

	// Transformは現在地のスナップショットとしてコピーする
	commands.emplace_back(SkinningModelRenderCommand{ _animInstance, _transform, _drawMaterialOverride });
	return true;
}
