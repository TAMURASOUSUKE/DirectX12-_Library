#include "DebugLogs.h"
#include "FrameDebugOverlay.h"

void FrameDebugOverlay::Build(const FrameDebugSystem& _system)
{
	overlayFrame.textCommands.clear(); // 構築前にリセット

	const auto& channels{ _system.GetChannels() };
	const auto& descriptors{ _system.GetMetricDescriptors() };
	const auto& values{ _system.GetReadFrame().metrics };

	// 同じ添え字で対応しているので個数が違えばエラー
	if (descriptors.size() != values.size())
	{
		DEBUG_LOG_ERROR("Matric定義と値の個数が一致しません\n");
		return;
	}

	// 最大でChannel見出しと全Metricを1個ずつ登録する
	overlayFrame.textCommands.reserve(channels.size() + descriptors.size());

	Vector2 cursor{ 16.0f, 16.0f };

	constexpr float TEXT_SCALE{ 0.75f };
	constexpr float LINE_HEIGHT{ 20.0f };
	constexpr float CHANNEL_GAP{ 8.0f };

	// 色を区別して見やすいようにする
	const Vector4 channelColor{ 0.2f, 0.85f, 1.0f, 1.0f };
	const Vector4 writtenColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	const Vector4 notWrittenColor{ 0.55f, 0.55f, 0.55f, 1.0f };

	for (const DebugChannelData& channel : channels)
	{
		// 無効化されているChannelは表示しない
		if (!channel.enabled) continue;

		// Channel見出しを作る
		char channelLine[256]{};

		std::snprintf(channelLine, sizeof(channelLine), "[%s]", channel.name.c_str());
		overlayFrame.textCommands.emplace_back(DebugTextCommand{ channelLine, cursor, TEXT_SCALE, channelColor });

		cursor.y += LINE_HEIGHT;

		// 現在のChannelに所属するMetricを探す
		for (std::size_t i = 0; i < descriptors.size(); ++i)
		{
			const DebugMetricDescriptor& descriptor{ descriptors[i] };
			const DebugMetricValue& value{ values[i] };

			const DebugChannelData* ownerChannel{ _system.FindChannel(descriptor.channelID) };

			// 現在表示中のChannelに所属していない
			if (ownerChannel != &channel) continue;

			char metricLine[256]{};

			if (value.written) std::snprintf(metricLine, sizeof(metricLine), "  %s : %.2f", descriptor.name.c_str(), value.value);
			// 登録済みだが、このフレームでは値が提出されていない
			else std::snprintf(metricLine, sizeof(metricLine), "  %s : --", descriptor.name.c_str());

			overlayFrame.textCommands.emplace_back(DebugTextCommand{ metricLine, cursor, TEXT_SCALE, value.written ? writtenColor : notWrittenColor });

			cursor.y += LINE_HEIGHT;
		}

		// 次のChannelとの間隔
		cursor.y += CHANNEL_GAP;
	}

}
