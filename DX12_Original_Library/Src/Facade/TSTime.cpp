#include "../Graphics/GraphicsDevice.h" // VSync制御で使う
#include "../FPS/FPSConstant.h"
#include "../FPS/FPSSystem.h"
#include "TimeInternal.h"
#include "TSTime.h"

namespace {
	// 時間管理システムの実体はFacade内部だけで所有する
	FPSSystem fpsSystem{};
}

void TimeInternal::Initialize()
{
	fpsSystem.Setup(TARGET_FPS);
}

void TimeInternal::BeginFrame()
{
	fpsSystem.BeginFrame();
}

void TimeInternal::EndFrame()
{
	fpsSystem.EndFrame();
}

void TimeInternal::Finish()
{
	fpsSystem.Finish();
}

void TimeInternal::SetVSync(bool _isVSyncEnabled)
{
	GraphicsDevice::Instance().SetVSync(_isVSyncEnabled);
}

float Time::UnscaledDeltaTime()
{
	return fpsSystem.GetUnscaledDeltaTime();
}

float Time::DeltaTime()
{
	return fpsSystem.GetDeltaTime();
}

void Time::SetTimeScale(float _timeScale)
{
	fpsSystem.SetTimeScale(_timeScale);
}

float Time::GetTimeScale()
{
	return fpsSystem.GetTimeScale();
}

float Time::FixedDeltaTime()
{
	return fpsSystem.GetFixedDeltaTime();
}

float Time::FPS()
{
	return fpsSystem.GetCurrentFPS();
}

float Time::Alpha()
{
	return fpsSystem.GetAlpha();
}

bool Time::IsFixedUpdateRequired()
{
	return fpsSystem.IsFixedUpdateRequired();
}

void Time::ConsumeFixedTime()
{
	fpsSystem.ConsumeFixedTime();
}

void Time::SetTargetFPS(int _targetFPS)
{
	const int target{ (std::max)(0, _targetFPS) };

	fpsSystem.SetTargetFPS(target);

	const bool useVSync{ target == 0 };
	TimeInternal::SetVSync(useVSync);
}

int Time::GetTargetFPS()
{
	return fpsSystem.GetTargetFPS();
}
