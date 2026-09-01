#pragma once
#include <d3d12.h>
#include "../Core/Handle/ModelHandle.h"
#include "RingConstantBuffer.h"

// 不完全な型でよいかつ、外部に参照を漏らさないために前方宣言
class Transform;
class ShaderSystem;
class CameraSystem;
class LightSystem;
struct AnimInstanceData;

// 3DモデルのGPU描画命令を送るクラス
class ModelRenderer
{
public:
	// 初期化
	bool Setup(ShaderSystem* _shaderSystem, CameraSystem* _cameraSystem, LightSystem* _lightSystem); // 初期化

	// 終了処理
	void Shutdown();
	// 最初のフレームで行う処理
	void BeginFrame();

	void DrawSkinnedModel(const AnimInstanceData& _anim, const Transform& _transform); // スキンメッシュ付きモデルのロード
	void DrawStaticModel(ModelHandle _model, const Transform& _transform); // 静的モデルの描画

private:
	// フレームデータを1度だけ転送するヘルパー
	D3D12_GPU_VIRTUAL_ADDRESS GetSceneFrameGPUAddress();

private:
	// 所有しない依存先
	ShaderSystem* shaderSystem{ nullptr };
	CameraSystem* cameraSystem{ nullptr };
	LightSystem* lightSystem{ nullptr };

	// ModelRendererが所有するGPU転送領域
	RingConstantBuffer sceneFrameRingCBV{};
	RingConstantBuffer modelObjectRingCBV{};
	RingConstantBuffer skinningRingCBV{};
	RingConstantBuffer materialRingCBV{};

	D3D12_GPU_VIRTUAL_ADDRESS sceneFrameGPUAddress{ 0 };

};
