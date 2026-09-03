#pragma once
#include <vector>
#include "../Animation/AnimationSystem.h"
#include "CameraSystem.h"
#include "../Component/Transform.h"
#include "ModelRenderer.h"
#include "ModelRenderQueue.h"
#include "ModelDrawPacket.h"

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

	// 登録された描画命令をサブメッシュ単位のPacketへ展開して描画順に並べる
	void BuildDrawPackets();
	// 不透明・マスクモデルを描画する
	void FlushOpaque();
	// 半透明モデルを奥から手前に描画する
	void FlushBlend();

	// 静的モデルの描画依頼登録
	bool Register(ModelHandle _model, const Transform& _transform);
	// スキニングモデルの描画依頼登録
	bool Register(AnimInstanceHandle _animInstance, const Transform& _transform);

private:
	// 渡されたデータから静的かスキニングかを判断してサブメッシュへ展開する
	bool AppendDrawPackets(const ModelData& _model, const AnimInstanceData* _animation, const Transform& _transform);

private:
	ModelRenderQueue renderQueue{};
	ModelRenderer renderer{};

	AnimationSystem* animationSystem{ nullptr }; 	// アニメーションシステム自体は所有せずにアニメーション個体の解決に使用する
	CameraSystem* cameraSystem{ nullptr }; // カリングや半透明のソートなどに使うカメラ距離を取るための参照

	std::vector<ModelDrawPacket> opaqueModels{}; // 不透明モデル描画用
	std::vector<ModelDrawPacket> blendModels{}; // 半透明モデルなどの描画用

	std::size_t droppedPacketCount{ 0 }; // 上限からあふれた数
};
