#include <cmath>
#include <cstdio>
#include "DebugLogs.h"
#include "FrameDebugOverlay.h"

namespace {
	// 種類から値の整形を行う
	void FormatMetricValue(char* _destination, std::size_t _destinationSize, DebugMetricUnit _unit, double _value)
	{
		// 書き込み先が存在しない、または容量が0なら書き込めない
		if (_destination == nullptr || _destinationSize == 0) return;

		// 途中で処理が終了しても空文字列として扱えるようにする
		_destination[0] = '\0';

		switch (_unit)
		{
		case DebugMetricUnit::None:
		{
			std::snprintf(_destination, _destinationSize, "%.2f", _value);
			break;
		}
		case DebugMetricUnit::Count:
		{
			// Countは整数として表示する%.0fは小数点以下を四捨五入して表示する
			std::snprintf(_destination, _destinationSize, "%.0f", _value);
			break;
		}
		case DebugMetricUnit::Bytes:
		{
			constexpr const char* BYTE_UNITS[]
			{
				"B",
				"KB",
				"MB",
				"GB",
				"TB"
			};

			constexpr std::size_t BYTE_UNIT_COUNT{ sizeof(BYTE_UNITS) / sizeof(BYTE_UNITS[0]) };

			double displayValue{ _value };
			std::size_t unitIndex{ 0 };

			// 1024以上なら値を小さくして、次の単位へ進める絶対値を見ることで負数でも無限ループしない
			while (std::fabs(displayValue) >= 1024.0 && unitIndex + 1 < BYTE_UNIT_COUNT)
			{
				displayValue /= 1024.0;
				unitIndex++;
			}

			if (unitIndex == 0) std::snprintf(_destination, _destinationSize, "%.0f %s", displayValue, BYTE_UNITS[unitIndex]); // Byte単位では基本的に整数表示
			else std::snprintf(_destination, _destinationSize, "%.2f %s", displayValue, BYTE_UNITS[unitIndex]);

			break;
		}
		case DebugMetricUnit::Percent:
		{
			// 提出側が0～100の値を渡す %自体を表示する場合は%%と書く
			std::snprintf(_destination, _destinationSize, "%.1f %%", _value);
			break;
		}
		case DebugMetricUnit::Distance:
		{
			// ライブラリでは1単位をメートルと確定していないためuとする
			std::snprintf(_destination, _destinationSize, "%.2f u", _value);
			break;
		}
		case DebugMetricUnit::Speed:
		{
			std::snprintf(_destination, _destinationSize, "%.2f u/s", _value);
			break;
		}
		case DebugMetricUnit::Frequency:
		{
			std::snprintf(_destination, _destinationSize, "%.1f Hz", _value);
			break;
		}
		case DebugMetricUnit::Seconds:
		{
			std::snprintf(_destination, _destinationSize, "%.3f s", _value);
			break;
		}
		case DebugMetricUnit::Milliseconds:
		{
			std::snprintf(_destination, _destinationSize, "%.3f ms", _value);
			break;
		}
		default:
		{
			// 不正な列挙値でも未初期化文字列を渡さない
			std::snprintf(_destination, _destinationSize, "Invalid");
			break;
		}
		}
	}
}

void FrameDebugOverlay::Build(const FrameDebugSystem& _system)
{
	overlayFrame.textCommands.clear(); // 構築前にリセット

	if (!visible) { return; } // 表示しないならここで終わる

	const auto& channels{ _system.GetChannels() };
	const auto& descriptors{ _system.GetMetricDescriptors() };
	const auto& values{ _system.GetReadFrame().metrics };
	const auto& statistics{ _system.GetMetricStatistics() };

	// 同じ添え字で対応しているので個数が違えばエラー
	if (descriptors.size() != values.size() || descriptors.size() != statistics.size())
	{
		DEBUG_LOG_ERROR("Metric定義と値の個数が一致しません\n");
		return;
	}

	// 最大でChannel見出しと全Metricを1個ずつ登録する
	overlayFrame.textCommands.reserve(channels.size() + descriptors.size());

	Vector2 cursor{ 16.0f, 16.0f };

	constexpr float TEXT_SCALE{ 1.0f };
	constexpr float LINE_HEIGHT{ 28.0f };
	constexpr float CHANNEL_GAP{ 12.0f };

	// 色を区別して見やすいようにする
	const Vector4 channelColor{ 0.2f, 0.85f, 1.0f, 1.0f };
	const Vector4 writtenColor{ 1.0f, 1.0f, 1.0f, 1.0f };
	const Vector4 notWrittenColor{ 0.55f, 0.55f, 0.55f, 1.0f };

	// Channelが一つも登録されていない
	if (channels.empty()) return;

	// Channel数が変化しても範囲外にならないようにする
	selectedChannelIndex %= channels.size();

	// 選択中のChannelが無効なら、次の有効なChannelを探す
	if (!channels[selectedChannelIndex].enabled) SelectNextChannel(_system);

	// 全Channelが無効なら表示しない
	if (!channels[selectedChannelIndex].enabled) return;

	const DebugChannelData& selectedChannel{ channels[selectedChannelIndex] };

	// 無効化されているChannelは表示しない
	if (!selectedChannel.enabled) return;

	// Channel見出しを作る
	char channelLine[256]{};

	std::snprintf(channelLine, sizeof(channelLine), "[%s]", selectedChannel.name.c_str());
	overlayFrame.textCommands.emplace_back(DebugTextCommand{ channelLine, cursor, TEXT_SCALE, channelColor });

	cursor.y += LINE_HEIGHT;

	// 現在のChannelに所属するMetricを探す
	for (std::size_t i = 0; i < descriptors.size(); ++i)
	{
		const DebugMetricDescriptor& descriptor{ descriptors[i] };
		const DebugMetricValue& value{ values[i] };

		const DebugChannelData* ownerChannel{ _system.FindChannel(descriptor.channelID) };

		// 現在表示中のChannelに所属していない
		if (ownerChannel != &selectedChannel) continue;

		char metricLine[256]{};
		bool hasDisplayValue{ false };

		if (descriptor.displayMode == DebugMetricDisplayMode::WindowStatistics)
		{
			const DebugMetricStatistics& metricStatistics{ statistics[i] };
			if (metricStatistics.valid)
			{
				char latestText[64]{};
				char averageText[64]{};
				char maximumText[64]{};

				FormatMetricValue(latestText, sizeof(latestText), descriptor.unit, metricStatistics.latest);
				FormatMetricValue(averageText, sizeof(averageText), descriptor.unit, metricStatistics.average);
				FormatMetricValue(maximumText, sizeof(maximumText), descriptor.unit, metricStatistics.maximum);

				std::snprintf(metricLine, sizeof(metricLine), " %s : Latest %s / Avg %s / Max %s", descriptor.name.c_str(), latestText, averageText, maximumText);
				hasDisplayValue = true;
			}
		}
		else if (value.written)
		{
			char valueText[64]{};
			FormatMetricValue(valueText, sizeof(valueText), descriptor.unit, value.value);
			std::snprintf(metricLine, sizeof(metricLine), "  %s : %s", descriptor.name.c_str(), valueText);

			hasDisplayValue = true;
		}

		// Currentが未提出、またはStatisticsがまだ確定していない
		if (!hasDisplayValue) std::snprintf(metricLine, sizeof(metricLine), "  %s : --", descriptor.name.c_str());

		overlayFrame.textCommands.emplace_back(DebugTextCommand{ metricLine, cursor, TEXT_SCALE, hasDisplayValue ? writtenColor : notWrittenColor });
		cursor.y += LINE_HEIGHT;
	}

	// 次のChannelとの間隔
	cursor.y += CHANNEL_GAP;
}

void FrameDebugOverlay::SelectNextChannel(const FrameDebugSystem& _system)
{
	const auto& channels{ _system.GetChannels() };

	if (channels.empty())
	{
		selectedChannelIndex = 0;
		return;
	}

	const std::size_t channelCount{ channels.size() };

	// 無効なChannelを飛ばしながら次へ進む
	for (std::size_t checkedCount = 0; checkedCount < channelCount; checkedCount++)
	{
		selectedChannelIndex = (selectedChannelIndex + 1) % channelCount;

		if (channels[selectedChannelIndex].enabled) return;
	}
}

void FrameDebugOverlay::SelectPreviousChannel(const FrameDebugSystem& _system)
{
	const auto& channels{ _system.GetChannels() };

	if (channels.empty())
	{
		selectedChannelIndex = 0;
		return;
	}

	const std::size_t channelCount{ channels.size() };

	// size_tを負数にせず、無効なChannelを飛ばしながら前へ戻る
	for (std::size_t checkedCount = 0; checkedCount < channelCount; checkedCount++)
	{
		selectedChannelIndex = (selectedChannelIndex + channelCount - 1) % channelCount;

		if (channels[selectedChannelIndex].enabled) return;
	}
}
