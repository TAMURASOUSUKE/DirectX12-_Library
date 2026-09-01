#include <type_traits>
#include "ModelRenderSystem.h"

bool ModelRenderSystem::Setup(ShaderSystem* _shaderSystem, CameraSystem* _cameraSystem, LightSystem* _lightSystem, AnimationSystem* _animationSystem)
{
	if (!_shaderSystem || !_cameraSystem || !_lightSystem || !_animationSystem) return false;
	animationSysmtem = _animationSystem;
	renderer.Setup( _shaderSystem, _cameraSystem, _lightSystem);
	return true;
}

void ModelRenderSystem::Shutdown()
{
	renderer.Shutdown();
	renderQueue.Shutdown();
}

void ModelRenderSystem::BeginFrame()
{
	renderQueue.Reset(); // 保存依頼をフレームの先頭でリセット
	renderer.BeginFrame(); // 各CBVのリセットなど
}

void ModelRenderSystem::Flush()
{
	for (const ModelRenderCommand& renderCommand : renderQueue.GetCommands())
	{
		// visitを使って静的モデルかスキニングモデルかを分けて描画命令を行う
		std::visit(
			[this](const auto& _command)
			{
				using CommandType = std::decay_t<decltype(_command)>; // constと参照を外すdecay_tでconstと参照を外してdecltypeでconstと参照を抜きにして読み込む

				// constexprを使うことでコンパイル時に分岐させる
				if constexpr (std::is_same_v<CommandType, StaticModelRenderCommand>)
				{
					renderer.DrawStaticModel(_command.modelHandle, _command.transform);
				}
				else if constexpr (std::is_same_v<CommandType, SkinningModelRenderCommand>)
				{
					AnimInstanceData* instance{ animationSysmtem->Lookup(_command.animInstanceHandle) };

					if (!instance) return;

					renderer.DrawSkinnedModel(*instance, _command.transform);
				}
			},
			renderCommand
		);
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
