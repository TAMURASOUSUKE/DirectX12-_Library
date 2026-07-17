#include <algorithm>
#include "FPSConstant.h"
#include "FPSSystem.h"

void FPSSystem::Setup(int _targetFPS)
{
	frameStartTime = {};
	prevFrameStartTime = {};

	unscaledDeltaTime = 0.0f;
	deltaTime = 0.0f;
	timeScale = 1.0f;

	currentFPS = 0.0f;
	fixedDeltaTime = FIXED_DELTA_TIME;
	accumulator = 0.0f;
	alpha = 0.0f;

	counter.Reset();
	limiter.Initialize(_targetFPS);
}

void FPSSystem::BeginFrame()
{
	const auto now{ std::chrono::steady_clock::now() };

	frameStartTime = now;

	// 初回フレームでは前回時刻が存在しないため計算しない
	if (prevFrameStartTime.time_since_epoch().count() != 0)
	{
		const std::chrono::duration<float> elapsed{ now - prevFrameStartTime };

		// ブレークポイントや処理落ちによって
		// DeltaTimeが極端に大きくなるのを防ぐ
		unscaledDeltaTime = std::clamp(elapsed.count(), 0.0f, MAX_DELTA_TIME);

		// ゲーム時間へTimeScaleを適用する
		deltaTime = unscaledDeltaTime * timeScale;
	}

	prevFrameStartTime = now;

	// 固定更新用に時間を蓄積する
	accumulator += deltaTime;
	accumulator = (std::min)(accumulator, MAX_FIXED_ACCUMULATOR);

	CalculateAlpha();
}

void FPSSystem::EndFrame()
{
	// 描画などに使った時間を差し引き、残り時間だけ待機する
	limiter.Wait(frameStartTime);

	// 待機終了後の時刻を使って実際のFPSを計測する
	counter.Update(std::chrono::steady_clock::now());
	currentFPS = counter.GetCurrentFPS();
}

void FPSSystem::Finish()
{
	frameStartTime = {};
	prevFrameStartTime = {};

	unscaledDeltaTime = 0.0f;
	deltaTime = 0.0f;
	timeScale = 1.0f;

	currentFPS = 0.0f;
	fixedDeltaTime = 0.0f;
	accumulator = 0.0f;
	alpha = 0.0f;

	counter.Reset();
	limiter.Reset();
}

bool FPSSystem::IsFixedUpdateRequired() const
{
	return accumulator >= fixedDeltaTime && fixedDeltaTime > 0.0f;
}

void FPSSystem::ConsumeFixedTime()
{
	// 固定更新できるだけの時間がなければ消費しない
	if (!IsFixedUpdateRequired())
	{
		return;
	}

	accumulator -= fixedDeltaTime;

	// 浮動小数点誤差によって負になるのを防ぐ
	accumulator = (std::max)(accumulator, 0.0f);

	CalculateAlpha();
}

void FPSSystem::CalculateAlpha()
{
	if (fixedDeltaTime <= 0.0f)
	{
		alpha = 0.0f;
		return;
	}

	// 固定更新後に余った時間が次の固定更新までの何割かを表す
	alpha = std::clamp(accumulator / fixedDeltaTime, 0.0f,1.0f);
}

void FPSSystem::SetTargetFPS(int _targetFPS)
{
	// FPS制限だけを変更し、DeltaTimeなどはリセットしない
	limiter.SetTargetFPS(_targetFPS);
}

void FPSSystem::SetTimeScale(float _timeScale)
{
	// 負数による逆再生は行わない
	timeScale = (std::max)(_timeScale, 0.0f);
}