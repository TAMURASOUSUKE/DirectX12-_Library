#pragma once
#include <chrono>

struct CPUFrameTimingFrame
{
	double milliseconds{ 0.0 }; // 経過のミリ秒
	int targetFPS{ 0 }; // 目標FPS
	bool valid{ false }; // 有効な計測結果か
};

class CPUFrameTimingSystem
{
public:
	// CPUフレーム計測を開始する
	void BeginFrame();

	// CPUフレーム計測を終了する
	void EndFrame(int _targetFPS);

	// 最後に完了した計測結果を取得する
	const CPUFrameTimingFrame& GetLastFrame() const;

private:
	std::chrono::steady_clock::time_point startTime{};
	CPUFrameTimingFrame lastFrame{};
	bool isActive{ false };
};
