#include <algorithm>
#include <intrin.h>
#include <thread>
#include "FPSConstant.h"
#include "FPSLimiter.h"

void FPSLimiter::Initialize(int _targetFPS)
{
	Reset();

	// 0以下ならFPS制限を行わない
	if (_targetFPS <= 0)
	{
		return;
	}

	// 整数除算で0マイクロ秒にならないよう、最低値を1にする
	const long long durationMicroseconds{(std::max)(1LL, MICROSECONDS_PER_SECOND / static_cast<long long>(_targetFPS))};

	targetDuration = std::chrono::microseconds{ durationMicroseconds };
}

void FPSLimiter::Reset()
{
	targetDuration = std::chrono::microseconds::zero();
	nextFrameTime = {};
}

void FPSLimiter::Wait(
	const std::chrono::steady_clock::time_point& _frameStartTime)
{
	// FPS制限が無効なら待機しない
	if (targetDuration <= std::chrono::microseconds::zero())
	{
		return;
	}

	// 初回はフレーム開始時刻から目標時刻を作る
	if (nextFrameTime.time_since_epoch().count() == 0)
	{
		nextFrameTime = _frameStartTime + targetDuration;
	}

	auto now{ std::chrono::steady_clock::now() };

	// 処理時間がすでに目標時間を超えている場合
	if (now >= nextFrameTime)
	{
		// 過去の遅れを追いかけず、現在時刻から予定を作り直す
		nextFrameTime = now + targetDuration;
		return;
	}

	const auto remainingTime{ nextFrameTime - now };
	const auto sleepMargin{ std::chrono::milliseconds{ 1 } };

	// 目標時刻の少し手前まではOSに処理を返して待機する
	if (remainingTime > sleepMargin)
	{
		std::this_thread::sleep_for(remainingTime - sleepMargin);
	}

	// 最後の短い時間だけ空回しして精度を上げる
	while (std::chrono::steady_clock::now() < nextFrameTime)
	{
		_mm_pause(); // 待機させて少しだけ最適化を図る
	}

	// 次フレームの目標時刻へ進める
	nextFrameTime += targetDuration;
}