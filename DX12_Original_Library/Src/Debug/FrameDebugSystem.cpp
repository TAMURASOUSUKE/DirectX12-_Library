#include <limits>
#include "DebugLogs.h"
#include "FrameDebugSystem.h"

DebugChannelID FrameDebugSystem::RegisterChannel(const std::string& _channelName)
{
	if (_channelName.empty())
	{
		DEBUG_LOG_ERROR("Channel登録に空文字が渡されました\n");
		return DebugChannelID{}; // 空 = 失敗の値を返す
	}

	// 同名があるかチェック
	for (std::size_t i = 0; i < channels.size(); i++)
	{
		if (channels[i].name == _channelName) return DebugChannelID{ static_cast<uint32_t>(i) }; // 同名ならそれのIDを返す
	}

	// Channel IDはuint32_tなので 表現できる登録数を超える場合は追加できない
	if(channels.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
	{
		DEBUG_LOG_ERROR("Channelの登録数が上限に達しました");
		return DebugChannelID{};
	}

	// 追加前のsizeが新しく追加される要素の添字になる
	const std::uint32_t newIndex{ static_cast<std::uint32_t>(channels.size()) };
	channels.push_back(DebugChannelData{_channelName, true});
	return DebugChannelID{ newIndex };
}

DebugMetricID FrameDebugSystem::RegisterMetric(const DebugMetricDescriptor& _descriptor)
{
	if (_descriptor.name.empty())
	{
		DEBUG_LOG_ERROR("Metric登録に空文字が渡されました\n");
		return DebugMetricID{}; // 空 = 失敗の値を返す
	}

	if (!_descriptor.channelID.IsValid())
	{
		DEBUG_LOG_ERROR("チャンネルIDが無効です\n");
		return DebugMetricID{};
	}

	if (channels.size() <= static_cast<std::size_t>(_descriptor.channelID.value))
	{
		DEBUG_LOG_ERROR("チャンネルIDが範囲外です\n");
		return DebugMetricID{};
	}

	// 同名があるかチェック
	for (std::size_t i = 0; i < metricDescriptors.size(); i++)
	{
		if (metricDescriptors[i].name != _descriptor.name) continue; // 同名じゃない
		if (metricDescriptors[i].channelID != _descriptor.channelID) continue; // チャンネルIDが同名じゃない
		// 同じChannel・同じ名前なのに設定が違う場合は設計の矛盾
		if (metricDescriptors[i].unit != _descriptor.unit || metricDescriptors[i].aggregation != _descriptor.aggregation)
		{
			DEBUG_LOG_ERROR("同名Metricに異なる設定が指定されました");
			return DebugMetricID{};
		}

		return DebugMetricID{ static_cast<uint32_t>(i) }; // 同名ならそれのIDを返す
	}

	// DebugMetricIDはuint32_tなので 表現できる登録数を超える場合は追加できない
	if (metricDescriptors.size() >= static_cast<std::size_t>(std::numeric_limits<std::uint32_t>::max()))
	{
		DEBUG_LOG_ERROR("Metricの登録数が上限に達しました");
		return DebugMetricID{};
	}
	// 追加前のsizeが新しく追加される要素の添字になる
	const std::uint32_t newIndex{ static_cast<std::uint32_t>(metricDescriptors.size()) };
	metricDescriptors.push_back(_descriptor);
	// metric保存用の場所を予約するために空を入れる
	writeFrame.metrics.emplace_back();
	readFrame.metrics.emplace_back();
	return DebugMetricID{ newIndex };
}
