#pragma once
#include "../Animation/AnimationSystem.h"
#include "ModelRenderer.h"
#include "ModelRenderQueue.h"

// モデルを描画するためのシステムを構築する
class ModelRenderSystem
{
public:
	// 各システムと接続する
	bool Setup(ShaderSystem* _shaderSystem, CameraSystem* _cameraSystem, LightSystem* _lightSystem, AnimationSystem* _animationSystem);

	// 終了処理
	void Shutdown();
	// フレームの最初の処理
	void BeginFrame();
	// 描画命令実行
	void Flush();

	// 静的モデルの描画依頼登録
	bool Register(ModelHandle _model, const Transform& _transform);
	// スキニングモデルの描画依頼登録
	bool Register(AnimInstanceHandle _animInstance, const Transform& _transform);

private:
	ModelRenderQueue renderQueue{};
	ModelRenderer renderer{};

	// アニメーションシステム自体は所有せずにアニメーション個体の解決に使用する
	AnimationSystem* animationSysmtem{ nullptr };
};
