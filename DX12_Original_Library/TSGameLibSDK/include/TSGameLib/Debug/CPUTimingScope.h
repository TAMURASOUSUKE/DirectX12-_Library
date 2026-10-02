#pragma once
#include <chrono>
#include "FrameDebugType.h"


class CPUTimingScope;

// 既存DebugFacadeに追加する生成関数の前方宣言
namespace Debug
{
	// 計測を開始する
	CPUTimingScope BeginCPUTiming(DebugMetricID _metricID);
}

// CPU処理時間をスコープ単位で計測する
class CPUTimingScope
{
public:
	~CPUTimingScope();

	// 計測結果の二重提出を防ぐ
	CPUTimingScope(const CPUTimingScope& _other) = delete;
	CPUTimingScope& operator=(const CPUTimingScope& _other) = delete;
	CPUTimingScope(CPUTimingScope&& _other) = delete;
	CPUTimingScope& operator=(CPUTimingScope&& _other) = delete;

private:
	// 関ポを使って軽量に計測結果をMetricへ提出する型を定義する
	using SubmitFunction = void(*)(DebugMetricID, double);

	// DebugFacadeが計測を開始できる
	CPUTimingScope(DebugMetricID _metricID, SubmitFunction _submitFunction);

	DebugMetricID metricID{}; // 計測結果の提出先
	SubmitFunction submitFunction{ nullptr }; // 結果を提出する関数
	std::chrono::steady_clock::time_point startTime{}; // 計測開始時刻

	// 既存のDebugFacadeだけコンストラクタを呼べるようにする
	friend CPUTimingScope Debug::BeginCPUTiming(DebugMetricID _metricID);
};
