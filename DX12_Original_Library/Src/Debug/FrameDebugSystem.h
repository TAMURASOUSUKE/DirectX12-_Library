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

	// 新しいフレームの計測を開始する
	void BeginFrame();

	// 収集を完了し、表示用Frameとして公開する
	void EndFrame();

	// チャンネルを登録してIDを返す
	DebugChannelID RegisterChannel(const std::string& _channelName);
	// 数値項目を登録してIDを返す
	DebugMetricID RegisterMetric(const DebugMetricDescriptor& _descriptor);
	// 登録済みMetricへ、このフレームの値を提出する
	void SubmitMetric(DebugMetricID _metricID, double _value);

	// 登録済みChannelを読み取り専用で取得する
	const std::vector<DebugChannelData>& GetChannels() const;

	// 登録済みMetricの定義を読み取り専用で取得する
	const std::vector<DebugMetricDescriptor>& GetMetricDescriptors() const;

	// 完成済みFrameを読み取り専用で取得する
	const DebugFrameData& GetReadFrame() const;

private:
	std::vector<DebugChannelData> channels{}; // 分類
	std::vector<DebugMetricDescriptor> metricDescriptors{}; // 提供された変わらない情報
	DebugFrameData writeFrame{}; // 書き込むフレームのデータ
	DebugFrameData readFrame{}; // 読み込むフレームのデータ

};
