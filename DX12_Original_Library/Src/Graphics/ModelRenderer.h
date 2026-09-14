#pragma once
#include <d3d12.h>
#include "../Core/Handle/ModelHandle.h"
#include "ModelDrawPacket.h"
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

	// 個体データでGPUアドレスを共有させるための準備関数
	bool PrepareModelData(const Transform& _transform, const AnimInstanceData* _animation, PreparedModelDrawData& _outData);

	// モデル全体で共通する状態を設定する
	bool BeginModelDraw(D3D12_GPU_VIRTUAL_ADDRESS _shadowFrameAddress, D3D12_GPU_DESCRIPTOR_HANDLE _shadowMapSRV);
	// パケット内のサブメッシュを1つ描画する
	bool DrawSubMesh(const ModelDrawPacket& _packet);

	// ShadowPassで使用する共通状態と光源ViewProjectionを設定する
	bool BeginShadowDraw(D3D12_GPU_VIRTUAL_ADDRESS _shadowFrameAddress);

	// パケット内のサブメッシュをShadowMapへ描画する
	bool DrawShadowSubMesh(const ModelDrawPacket& _packet);

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
	RingConstantBuffer materialRingCBV{}; // glTF/PBR用MaterialCB
	RingConstantBuffer userMaterialParameterRingCBV{}; // ユーザー定義値

	D3D12_GPU_VIRTUAL_ADDRESS sceneFrameGPUAddress{ 0 };
	D3D12_GPU_VIRTUAL_ADDRESS zeroMaterialParameterAddress{ 0 };

};
