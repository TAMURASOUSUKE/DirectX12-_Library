#pragma once
#include <vector>
#include "FrameDebugType.h"

// 1フレームの間に提出されたデータを管理し画面に表示するための準備を行うシステム
class FrameDebugSystem
{
public:
	FrameDebugSystem() = default;
	~FrameDebugSystem() = default;

	FrameDebugSystem(const FrameDebugSystem& _other) = delete;
	FrameDebugSystem& operator=(const  FrameDebugSystem& _other) = delete;

	// チャンネルを登録してIDを返す
	DebugChannelID RegisterChannel(const std::string& _channelName);
	// 数値項目を登録してIDを返す
	DebugMetricID RegisterMetric(const DebugMetricDescriptor& _descriptor);

private:
	std::vector<DebugChannelData> channels{}; // 分類
	std::vector<DebugMetricDescriptor> metricDescriptors{}; // 提供された変わらない情報
	DebugFrameData writeFrame{}; // 書き込むフレームのデータ
	DebugFrameData readFrame{}; // 読み込むフレームのデータ

};
