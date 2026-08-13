#include <cmath>
#include "../Debug/DebugLogs.h"
#include "LightSystem.h"

namespace
{
	// C++とHLSLで同期させるため16byte単位の配置にするGPU専用データ
	struct SceneLightCB
	{
		// xyz : 光線が進む方向 w : 光の強さ
		Vector4 directionalDirectionAndIntensity{};
		// xyz : 平行光源の色 w : 未使用
		Vector4 directionalColor{};
		// xyz : 環境光の色 w : 環境光の強さ
		Vector4 ambientColorAndIntensity{};
	};
	static_assert(sizeof(SceneLightCB) == 48, "SceneLightCBのサイズがHLSL側の配置と一致しません");
}

namespace
{
	// vector3の有限値を確かめるヘルパー
	bool IsFiniteVector3(const Vector3& _value) { return std::isfinite(_value.x) && std::isfinite(_value.y) && std::isfinite(_value.z); }
	// Vector内に負の値が含まれていないかチェックするヘルパー
	bool IsNonNegativeColor(const Vector3& _color) { return _color.x >= 0.0f && _color.y >= 0.0f && _color.z >= 0.0f; }
}

bool LightSystem::Setup()
{
	// シーン全体で共有するため1フレーム1回分だけ確保
	lightRingBuffer.Setup(static_cast<UINT>(sizeof(SceneLightCB)), 1);
	if (lightRingBuffer.GetCurrentVirtualAddress() == 0)
	{
		DEBUG_LOG_ERROR("ライト用RingConstantBufferの作成に失敗しました\n");
		return false;
	}
	frameGPUAddress = 0;
	return true;
}

void LightSystem::Shutdown()
{
	lightRingBuffer.Shutdown();
	frameGPUAddress = 0;
	currentLight = {};
}

void LightSystem::BeginFrame()
{
	lightRingBuffer.Reset();
	frameGPUAddress = 0;
}

bool LightSystem::SetSceneLight(const SceneLight& _sceneLight)
{
	const DirectionalLight& directional{ _sceneLight.directional };
	const AmbientLight& ambient{ _sceneLight.ambient };
	if (!IsFiniteVector3(directional.direction) || directional.direction.LengthSquared() <= Math::EPSILON * Math::EPSILON)
	{
		DEBUG_LOG_ERROR("DirectionlLightの方向が不正です\n");
		return false;
	}
	if(!IsFiniteVector3(directional.color) || !IsNonNegativeColor(directional.color) || !std::isfinite(directional.intensity) || directional.intensity < 0.0f)
	{
		DEBUG_LOG_ERROR("DirectionalLightの色または強度が不正です\n");
		return false;
	}
	if (!IsFiniteVector3(ambient.color) || !IsNonNegativeColor(ambient.color) || !std::isfinite(ambient.intensity) || ambient.intensity < 0.0f)
	{
		DEBUG_LOG_ERROR("AmbientLightの色または強度が不正です\n");
		return false;
	}
	currentLight = _sceneLight;
	return true;
}

D3D12_GPU_VIRTUAL_ADDRESS LightSystem::GetFrameGPUAddress()
{
	// すでに現在フレーム用のデータを送っていれば再利用する
	if (frameGPUAddress != 0) return frameGPUAddress;

	const Vector3 normalizedDirection{ Vector3::Normalized(currentLight.directional.direction) };
	SceneLightCB lightData{};

	lightData.directionalDirectionAndIntensity = { normalizedDirection.x, normalizedDirection.y, normalizedDirection.z, currentLight.directional.intensity};
	lightData.directionalColor = { currentLight.directional.color.x, currentLight.directional.color.y, currentLight.directional.color.z, 0.0f };
	lightData.ambientColorAndIntensity = { currentLight.ambient.color.x, currentLight.ambient.color.y, currentLight.ambient.color.z, currentLight.ambient.intensity };

	frameGPUAddress = lightRingBuffer.Update(&lightData, static_cast<UINT>(sizeof(SceneLightCB)));
	if (frameGPUAddress == 0) DEBUG_LOG_ERROR("ライトデータのGPU転送に失敗しました\n");
	return frameGPUAddress;
}
