#pragma once
#include <string>
#include "../Debug/FrameDebugType.h"

// ユーザーにマクロ以外のデバッグ機能を提供する空間
namespace Debug
{
	// デバッグを行う際、どの分類かを登録する
	DebugChannelID RegisterChannel(const std::string& _name);
	// 登録時から変わらない情報を持つデータを専用の構造体で作成して渡し、登録する
	DebugMetricID RegisterMetric(const DebugMetricDescriptor& _descriptor);
	// 指定したIDの場所に値を記録する
	void SubmitMetric(DebugMetricID _metricID, double _value);
	// Debug Overlay全体の表示を切り替える
	void SetOverlayVisible(bool _visible);
	// Debug Overlayが表示設定か取得する
	bool IsOverlayVisible();
	// 指定Channelの計測と表示を切り替える
	bool SetChannelEnabled(DebugChannelID _channelID, bool _enabled);
}
