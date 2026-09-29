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

	// 完成済みFrameを読み取り専用で取得する
	const DebugFrameData& GetReadFrame() const { return readFrame; }

	// IDに対応するChannelを読み取り専用で取得する
	const DebugChannelData* FindChannel(DebugChannelID _channelID) const;

private:
	// 提出先のチャンネルのチェックを行う
	bool CanSubmitToChannel(DebugChannelID _id) const;

private:
	std::vector<DebugChannelData> channels{}; // 分類
	std::vector<DebugMetricDescriptor> metricDescriptors{}; // 提供された変わらない情報
	DebugFrameData writeFrame{}; // 書き込むフレームのデータ
	DebugFrameData readFrame{}; // 読み込むフレームのデータ

};
