#include "CPUTimingScope.h"

CPUTimingScope::CPUTimingScope(DebugMetricID _metricID, SubmitFunction _submitFunction) : metricID{_metricID}, submitFunction{_submitFunction}
{
	// 提出先が有効な場合だけ時間を読む
	if (metricID.IsValid() && submitFunction) startTime = std::chrono::steady_clock::now(); // 現在時刻を開始時間とする
}

CPUTimingScope::~CPUTimingScope()
{
	// 無効な計測なら弾く
	if (!metricID.IsValid() || !submitFunction) return;

	const auto endTime{ std::chrono::steady_clock::now() };

	// 開始から終了までの時間をミリ秒単位のdoubleへ変換する
	const std::chrono::duration<double, std::milli> elapsed{ endTime - startTime };
	submitFunction(metricID, elapsed.count());
}
