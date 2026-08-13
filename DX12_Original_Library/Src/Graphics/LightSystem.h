#pragma once
#include "../Component/Light.h"
#include "RingConstantBuffer.h"

// 3D空間を照らすライトを管理する
class LightSystem
{
public:
	// ライト用RingConstantBufferを準備
	bool Setup();
	// リソースの開放
	void Shutdown(); 

	// フレーム用アップロード状態をリセットする
	void BeginFrame();
	// 現在使用するライト設定を変更する
	bool SetSceneLight(const SceneLight& _sceneLight);

	// 現在フレーム用のライトCBをGPUへ送りそのアドレスを返す
	D3D12_GPU_VIRTUAL_ADDRESS GetFrameGPUAddress();

private:
	SceneLight currentLight{};
	RingConstantBuffer lightRingBuffer{};
	D3D12_GPU_VIRTUAL_ADDRESS frameGPUAddress{ 0 }; // 同一フレームで何度呼ばれてもGPUアップロードは1回
};
