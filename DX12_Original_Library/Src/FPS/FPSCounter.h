#pragma once
#include <chrono>

// 一定フレーム間の経過時間からFPSを計測する
class FPSCounter
{
public:
	FPSCounter() = default;
	~FPSCounter() = default;

	FPSCounter(const FPSCounter& _other) = delete;
	FPSCounter& operator=(const FPSCounter& _other) = delete;

	// FPS計測状態を初期化する
	void Reset();

	// 現在時刻を使ってFPS計測を更新する
	void Update(const std::chrono::steady_clock::time_point& _currentTime);

	// 現在計測されているFPSを取得する
	float GetCurrentFPS() const { return currentFPS; }

private:
	std::chrono::steady_clock::time_point sampleStartTime{}; // 計測開始時刻
	int frameCounter{ 0 }; // 計測したフレーム数
	float currentFPS{ 0.0f }; // 現在のFPS
};