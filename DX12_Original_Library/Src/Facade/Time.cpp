#include "../FPS/FPSConstant.h"
#include "../FPS/FPSSystem.h"
#include "TimeInternal.h"
#include "Time.h"

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

float Time::DeltaTime()
{
	return fpsSystem.GetDeltaTime();
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
	fpsSystem.SetTargetFPS(_targetFPS);
}

int Time::GetTargetFPS()
{
	return fpsSystem.GetTargetFPS();
}