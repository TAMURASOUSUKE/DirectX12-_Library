#include "FPSConstant.h"
#include "FPSCounter.h"

void FPSCounter::Reset()
{
	sampleStartTime = {};
	frameCounter = 0;
	currentFPS = 0.0f;
}

void FPSCounter::Update(
	const std::chrono::steady_clock::time_point& _currentTime)
{
	// 初回は計測開始時刻だけを記録する
	if (sampleStartTime.time_since_epoch().count() == 0)
	{
		sampleStartTime = _currentTime;
		return;
	}

	++frameCounter;

	// 指定フレーム数が溜まるまでは計算しない
	if (frameCounter < FPS_SAMPLE_FRAME_COUNT)
	{
		return;
	}

	// 計測開始時刻から現在までの秒数を取得する
	const std::chrono::duration<float> elapsed{ _currentTime - sampleStartTime };

	if (elapsed.count() > 0.0f)
	{
		currentFPS = static_cast<float>(frameCounter) / elapsed.count();
	}

	// 次の計測に備えて状態を戻す
	frameCounter = 0;
	sampleStartTime = _currentTime;
}