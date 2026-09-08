#pragma once
#include "../Component/Light.h"
#include "../Component/Shadow.h"
#include "GraphicsType.h"

// 影描画に必要な行列とGPUリソースを管理
class ShadowSystem
{
public:
	ShadowSystem() = default;
	~ShadowSystem() = default;

	ShadowSystem(const ShadowSystem&) = delete;
	ShadowSystem& operator=(const ShadowSystem&) = delete;

	// ShadowMap用の深度テクスチャ,DSV,SRVを作成する
	bool Setup(ID3D12Device* _device, UINT _resolution);

	// ShadowSystemが所有するGPUリソースを解放する
	void Shutdown();

	// 平行光源の方向と影設定から光源視点の行列を更新する
	bool UpdateDirectionalLightMatrices(const DirectionalLight& _light, const DirectionalShadowSettings& _settings);

	// ShadowMapへ深度を書き込むパスを開始する
	bool BeginShadowPass(ID3D12GraphicsCommandList* _commandList);

	// ShadowMapへの書き込みを終了しPixelShaderから参照できる状態へ変更する
	bool EndShadowPass(ID3D12GraphicsCommandList* _commandList);

	// ShadowMapが使用可能な状態か
	bool IsReady() const { return shadowMap && shadowDSV.IsValid() && shadowSRV.IsValid(); }

	// ShadowMapの解像度
	UINT GetResolution() const { return resolution; }

	// 深度を書き込むときに使用するCPU側DSV
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() const { return shadowDSV.cpu; }

	// Pixel Shaderから深度を読むときに使用するGPU側SRV
	D3D12_GPU_DESCRIPTOR_HANDLE GetSRV() const { return shadowSRV.gpu; }

	// 光源から見たView行列
	const Mat4x4& GetLightViewMatrix() const { return lightViewMatrix; }

	// 光源用の正射影行列
	const Mat4x4& GetLightProjectionMatrix() const { return lightProjectionMatrix; }

	// 頂点を光源のクリップ空間へ変換する行列
	const Mat4x4& GetLightViewProjectionMatrix() const { return lightViewProjectionMatrix; }

private:
	// ShadowMap本体と対応するDSV・SRVを作る
	bool CreateShadowMap(UINT _resolution);

	// shadowMapのResourceStateを変更する
	bool TransitionResource(ID3D12GraphicsCommandList* _commandList, D3D12_RESOURCE_STATES _nextState);

private:
	ID3D12Device* device{ nullptr }; // 外から受け取るdevice
	ComPtr<ID3D12Resource> shadowMap{}; // 光源からみた進度を保存するGPUテクスチャ
	DescriptorHandle shadowDSV{}; // 深度を書き込むためのView
	DescriptorHandle shadowSRV{}; // Shaderから深度を読み込むためのView
	UINT resolution{ 0 }; // ShadowMapの縦横ピクセル数
	D3D12_RESOURCE_STATES resourceState{ D3D12_RESOURCE_STATE_DEPTH_WRITE }; // ResourceBarrierを正しく発行するため、現在のリソース状態を保持する

	Mat4x4 lightViewMatrix{ Mat4x4::Identity };
	Mat4x4 lightProjectionMatrix{ Mat4x4::Identity };
	Mat4x4 lightViewProjectionMatrix{ Mat4x4::Identity };

	bool isPassActive{ false }; // 深度パスが有効か
};
