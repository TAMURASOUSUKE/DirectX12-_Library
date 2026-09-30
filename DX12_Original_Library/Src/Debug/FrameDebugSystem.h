#pragma once
#include <chrono>
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
	// Lineのデバッグ描画依頼を提出
	void SubmitLine(const DebugLineCommand& _command);
	// Boxのデバッグ描画依頼を提出
	void SubmitBox(const DebugBoxCommand& _command);
	// Shpereのデバッグ描画依頼を提出
	void SubmitSphere(const DebugSphereCommand& _command);
	// Capsuleのデバッグ描画依頼を提出
	void SubmitCapsule(const DebugCapsuleCommand& _command);

	// 指定Channelの計測と表示を切り替える
	bool SetChannelEnabled(DebugChannelID _channelID, bool _enabled);

	// 登録済みChannelを読み取り専用で取得する
	const std::vector<DebugChannelData>& GetChannels() const { return channels; }

	// 登録済みMetricの定義を読み取り専用で取得する
	const std::vector<DebugMetricDescriptor>& GetMetricDescriptors() const { return metricDescriptors; }

	// 確定済みの時間方向統計を取得する
	const std::vector<DebugMetricStatistics>& GetMetricStatistics() const { return metricStatistics; }

	// 完成済みFrameを読み取り専用で取得する
	const DebugFrameData& GetReadFrame() const { return readFrame; }

	// IDに対応するChannelを読み取り専用で取得する
	const DebugChannelData* FindChannel(DebugChannelID _channelID) const;

private:
	// 一定期間のMetricを集計するための内部データ
	struct DebugMetricWindowAccumulator
	{
		double latest{ 0.0 }; // 最後に提出された値
		double sum{ 0.0 }; // 平均計算に使用する合計
		double maximum{ 0.0 }; // 区間内の最大値
		std::uint32_t sampleCount{ 0 }; // 提出された回数
	};

	// 提出先のチャンネルのチェックを行う
	bool CanSubmitToChannel(DebugChannelID _id) const;

private:
	static constexpr double STATISTICS_WINDOW_SECONDS{ 0.5 }; // 統計結果を確定する間隔
	std::vector<DebugChannelData> channels{}; // 分類
	std::vector<DebugMetricDescriptor> metricDescriptors{}; // 提供された変わらない情報
	DebugFrameData writeFrame{}; // 書き込むフレームのデータ
	DebugFrameData readFrame{}; // 読み込むフレームのデータ

	std::vector<DebugMetricWindowAccumulator> metricAccumulators{}; // Metricごとの計算途中の値
	std::vector<DebugMetricStatistics> metricStatistics{}; // Overlayへ公開する確定済みの統計
	std::chrono::steady_clock::time_point statisticsWindowStart{ std::chrono::steady_clock::now() }; // 現在の統計区間を開始した時刻

};
