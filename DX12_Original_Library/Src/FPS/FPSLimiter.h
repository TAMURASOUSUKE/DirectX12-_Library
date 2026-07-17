#pragma once
#include <chrono>

// 指定したFPSを超えないようにCPUを待機させる
class FPSLimiter
{
public:
	FPSLimiter() = default;
	~FPSLimiter() = default;

	FPSLimiter(const FPSLimiter& _other) = delete;
	FPSLimiter& operator=(const FPSLimiter& _other) = delete;

	// 目標FPSを設定する
	// 0以下を指定した場合はFPS制限を無効にする
	void Initialize(int _targetFPS);

	// FPS制御状態を初期化する
	void Reset();

	// フレーム開始時刻を基準に、目標時刻まで待機する
	void Wait(const std::chrono::steady_clock::time_point& _frameStartTime);

private:
	std::chrono::microseconds targetDuration{}; // 1フレームの目標時間
	std::chrono::steady_clock::time_point nextFrameTime{}; // 次の目標時刻
};