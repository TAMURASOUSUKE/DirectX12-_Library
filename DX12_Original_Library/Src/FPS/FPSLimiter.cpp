#include <algorithm>
#include <intrin.h>
#include <ratio>
#include "../Debug/DebugLogs.h"
#include "FPSConstant.h"
#include "FPSLimiter.h"

FPSLimiter::~FPSLimiter()
{
	Reset();
}

void FPSLimiter::Initialize(int _targetFPS)
{
	SetTargetFPS(_targetFPS);
}

void FPSLimiter::SetTargetFPS(int  _targetFPS)
{
	// 負数もFPS制限なし
	targetFPS = (std::max)(_targetFPS, 0);

	// 前のFPSで計算した目標時刻は使用できないので破棄
	nextFrameTime = {};

	// FPS制限がない場合は狙う経過時間をゼロに
	if (targetFPS == 0)
	{
		targetDuration = std::chrono::microseconds::zero();
		return;
	}

	// 指定したFPSから経過時間を求める
	const long long durationMicroseconds{ (std::max)(1LL, MICROSECONDS_PER_SECOND / static_cast<long long>(targetFPS)) };
	targetDuration = std::chrono::microseconds{ durationMicroseconds };
	// 失敗してもwait側のビジーループへフォールバックできる
	EnsureWaitableTime();
}

void FPSLimiter::Reset()
{
	targetFPS = 0;
	targetDuration = std::chrono::microseconds::zero();
	nextFrameTime = {};

	if (waitableTimer)
	{
		// 予約中だった場合に備え、キャンセル
		CancelWaitableTimer(waitableTimer);
		CloseHandle(waitableTimer);
		waitableTimer = nullptr;
	}
}

void FPSLimiter::Wait(const std::chrono::steady_clock::time_point& _frameStartTime)
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

	const auto spinMargin{ std::chrono::milliseconds{ 1 } };
	const auto remainingTime{ nextFrameTime - now };

	// 待機処理
	if (remainingTime > spinMargin && EnsureWaitableTime())
	{
		const auto timerWaitTime{ remainingTime - spinMargin };
		// SetWaitableTimerは100ナノ秒単位
		using HundredNanoseconds = std::chrono::duration<LONGLONG, std::ratio<1, 10'000'000>>;
		const LONGLONG relativeTicks{ std::chrono::duration_cast<HundredNanoseconds>(timerWaitTime).count() };

		if (relativeTicks > 0)
		{
			LARGE_INTEGER dueTime{};
			// 負数は現在時刻からの相対時間を表す
			dueTime.QuadPart = -relativeTicks;
			const BOOL timerSet{ SetWaitableTimer(waitableTimer, &dueTime, 0, nullptr, nullptr, false) }; // 周期タイマーにはしない

			if (timerSet)
			{
				const DWORD waitResult{ WaitForSingleObject(waitableTimer, INFINITE) };
				if (waitResult != WAIT_OBJECT_0) DEBUG_LOG_ERROR("WaitableTimerの待機に失敗しました error = {}\n", GetLastError());
			}
			else
			{
				DEBUG_LOG_ERROR("WaitableTiemrの設定に失敗しました error = {}\n", GetLastError());
			}
		}
	}

	// タイマー作成・設定に失敗した場合でも最終的にはビジーループで目標時刻を保証する
	while (std::chrono::steady_clock::now() < nextFrameTime)
	{
		_mm_pause(); // 待機させて少しだけ最適化を図る
	}

	const auto afterWait{ std::chrono::steady_clock::now() };

	// 次フレームの目標時刻へ進める
	nextFrameTime += targetDuration;

	// OS側の都合で1ms以上寝すぎた場合は、短いフレームで追いつこうとせずに予定を作り直す
	if (afterWait - (nextFrameTime - targetDuration) > spinMargin)
	{
		nextFrameTime = afterWait + targetDuration;
	}
}

bool FPSLimiter::EnsureWaitableTime()
{
	// すでに作成されている場合
	if (waitableTimer)
	{
		return true;
	}

	// 自動リセット方式の高精度WaitableTimerを作成する
	waitableTimer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_MODIFY_STATE | SYNCHRONIZE);
	if (!waitableTimer)
	{
		DEBUG_LOG_ERROR("高精度WaitableTimerの作成に失敗しましたerror = {}\n", GetLastError());
		return false;
	}
	return true;
}
