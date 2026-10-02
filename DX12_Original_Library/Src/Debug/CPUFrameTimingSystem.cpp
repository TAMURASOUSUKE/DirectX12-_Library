#include "CPUFrameTimingSystem.h"

void CPUFrameTimingSystem::BeginFrame()
{
	// EndFrame前に二重開始された場合は、元の開始時刻を保護する
	if (isActive) return;

	startTime = std::chrono::steady_clock::now();
	isActive = true;
}

void CPUFrameTimingSystem::EndFrame(int _targetFPS)
{
	if (!isActive) return;
	const std::chrono::steady_clock::time_point endTime{ std::chrono::steady_clock::now() };
	const std::chrono::duration<double, std::milli> elapsed{ endTime - startTime };

	lastFrame.milliseconds = elapsed.count();
	lastFrame.targetFPS = _targetFPS;
	isActive = false;
	lastFrame.valid = true;
}

const CPUFrameTimingFrame& CPUFrameTimingSystem::GetLastFrame() const
{
	return lastFrame;
}
