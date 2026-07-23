#pragma once
#include <chrono>
#include "FPSCounter.h"
#include "FPSLimiter.h"

// FPS、DeltaTime、固定更新時間をまとめて管理する
class FPSSystem
{
public:
	FPSSystem() = default;
	~FPSSystem() = default;

	FPSSystem(const FPSSystem& _other) = delete;
	FPSSystem& operator=(const FPSSystem& _other) = delete;

	// FPS制御を初期化する
	void Setup(int _targetFPS);

	// フレーム開始時にDeltaTimeなどを更新する
	void BeginFrame();

	// フレーム終了時にFPS制御とFPS計測を行う
	void EndFrame();

	// FPS制御状態を終了する
	void Finish();

	// 固定更新を実行する必要があるか
	bool IsFixedUpdateRequired() const;

	// 固定更新1回分の時間を蓄積時間から消費する
	void ConsumeFixedTime();
	
	void SetTimeScale(float _timeScale);
	void SetTargetFPS(int _targetFPS);
	int GetTargetFPS() const { return limiter.GetTargetFPS(); }
	float GetTimeScale() const { return  timeScale; }
	float GetUnscaledDeltaTime() const { return unscaledDeltaTime; }
	float GetDeltaTime() const { return deltaTime; }
	float GetFixedDeltaTime() const { return fixedDeltaTime; }
	float GetCurrentFPS() const { return currentFPS; }
	float GetAlpha() const { return alpha; }

private:
	// 固定更新後に残った時間の割合を計算する
	void CalculateAlpha();

private:
	std::chrono::steady_clock::time_point frameStartTime{}; // 現在フレームの開始時刻
	std::chrono::steady_clock::time_point prevFrameStartTime{}; // 前フレームの開始時刻

	FPSCounter counter{};
	FPSLimiter limiter{};

	float unscaledDeltaTime{ 0.0f }; // TimeScale適用前の実時間
	float deltaTime{ 0.0f }; // TimeScale適用後の前フレームからの経過秒数
	float timeScale{ 1.0f }; // ゲーム時間の進行倍率
	float currentFPS{ 0.0f }; // 現在のFPS
	float fixedDeltaTime{ 0.0f }; // 固定更新1回分の秒数
	float accumulator{ 0.0f }; // 固定更新用の蓄積時間
	float alpha{ 0.0f }; // 描画補間用の割合
};