#include <limits>
#include <utility>
#include <cmath>
#include "../Math/MathConstant.h"
#include "DebugLogs.h"
#include "FrameDebugSystem.h"

void FrameDebugSystem::BeginFrame()
{
	// 計測した値を空にしてデフォルト状態へ戻す
	for (DebugMetricValue& metricValue : writeFrame.metrics)
	{
		metricValue = DebugMetricValue{};
	}

	writeFrame.lineCommands.clear();
	writeFrame.boxCommands.clear();
	writeFrame.sphereCommands.clear();
	writeFrame.capsuleCommands.clear();
}

void FrameDebugSystem::EndFrame()
{
	// 書き込み用と表示用を入れ替える(古い保存領域も再利用できる)
	std::swap(writeFrame, readFrame);
}

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

void FrameDebugSystem::SubmitMetric(DebugMetricID _metricID, double _value)
{
	if (!_metricID.IsValid())
	{
		DEBUG_LOG_ERROR("IDが不正です\n");
		return;
	}

	// 冗長にならないようにキャッシュ
	const std::size_t index{ static_cast<std::size_t>(_metricID.value) };

	if (index >= metricDescriptors.size() || index >= writeFrame.metrics.size())
	{
		DEBUG_LOG_ERROR("IDが配列範囲外です");
		return;
	}

	const DebugMetricDescriptor& metricDesc{ metricDescriptors[index] };
	DebugMetricValue& metricValue{ writeFrame.metrics[index] };

	if (!metricDesc.channelID.IsValid())
	{
		DEBUG_LOG_ERROR("所属しているチャンネルが不正です\n");
		return;
	}

	const std::size_t channelIndex{ static_cast<std::size_t>(metricDesc.channelID.value) };

	if (channelIndex >= channels.size())
	{
		DEBUG_LOG_ERROR("所属Channelが配列範囲外です");
		return;
	}

	// 無効化されいているチャンネルなら終了
	if (!channels[channelIndex].enabled) return;

	switch (metricDesc.aggregation)
	{
	case DebugMetricAggregation::Set:
	{
		// 最後の値を適用する
		metricValue.value = _value;
		break;
	}
	case DebugMetricAggregation::Add:
	{
		// 加算する
		metricValue.value += _value;
		break;
	}
	case DebugMetricAggregation::Max:
	{
		// まだ一度も更新されていないもしくは現在の値より大きければ入れる
		if (!metricValue.written || metricValue.value < _value) metricValue.value = _value;
		break;
	}
	default:
		DEBUG_LOG_ERROR("不正な登録方法が指定されています\n");
		return;
	}

	// 正常に値が更新されたのでwrittenもtrueへ
	metricValue.written = true;
}

void FrameDebugSystem::SubmitLine(const DebugLineCommand& _command)
{
	// ID関連が正しいかチェック
	if (!CanSubmitToChannel(_command.channelID)) return;

	// 形状データチェック
	if (!Vector3::IsFinite(_command.start) || !Vector3::IsFinite(_command.end) || !Vector4::IsFinite(_command.color)) return;

	writeFrame.lineCommands.push_back(_command);
}

void FrameDebugSystem::SubmitBox(const DebugBoxCommand& _command)
{
	// ID関連が正しいかチェック
	if (!CanSubmitToChannel(_command.channelID)) return;

	// 形状データチェック
	if (!Vector3::IsFinite(_command.center) || !Vector3::IsFinite(_command.halfSize) || !Quaternion::IsFinite(_command.rotation) ||
		!Vector3::IsNonNegative(_command.halfSize) || !Vector4::IsFinite(_command.color) || std::abs(_command.rotation.LengthSquared() - 1.0f) > Math::EPSILON) return;

	writeFrame.boxCommands.push_back(_command);
}

void FrameDebugSystem::SubmitSphere(const DebugSphereCommand& _command)
{
	// ID関連が正しいかチェック
	if (!CanSubmitToChannel(_command.channelID)) return;

	// 形状データチェック
	if (!Vector3::IsFinite(_command.center) || !std::isfinite(_command.radius) || _command.radius <= Math::EPSILON || !Vector4::IsFinite(_command.color)) return;

	writeFrame.sphereCommands.push_back(_command);
}

void FrameDebugSystem::SubmitCapsule(const DebugCapsuleCommand& _command)
{
	// ID関連が正しいかチェック
	if (!CanSubmitToChannel(_command.channelID)) return;

	// 形状データチェック
	if (!Vector3::IsFinite(_command.start) || !Vector3::IsFinite(_command.end) || !std::isfinite(_command.radius) || _command.radius <= Math::EPSILON ||
		!Vector4::IsFinite(_command.color) || (_command.end - _command.start).LengthSquared() <= Math::EPSILON * Math::EPSILON) return;

	writeFrame.capsuleCommands.push_back(_command);
}

bool FrameDebugSystem::SetChannelEnabled(DebugChannelID _channelID, bool _enabled)
{
	if (!_channelID.IsValid())
	{
		DEBUG_LOG_ERROR("不正なIDが渡されました\n");
		return false;
	}

	const std::size_t index{ static_cast<std::size_t>(_channelID.value) };
	if (index >= channels.size())
	{
		DEBUG_LOG_ERROR("範囲外のIDが渡されました\n");
		return false;
	}

	// 表示状態を更新する
	channels[index].enabled = _enabled;
	return true;
}

const DebugChannelData* FrameDebugSystem::FindChannel(DebugChannelID _channelID) const
{
	if (!_channelID.IsValid())
	{
		DEBUG_LOG_ERROR("不正なIDが渡されました\n");
		return nullptr;
	}

	if (static_cast<std::size_t>(_channelID.value) >= channels.size())
	{
		DEBUG_LOG_ERROR("ChannelIDが配列外です\n");
		return nullptr;
	}

	// 指定されたIDからデータを取り出す
	return &channels[_channelID.value];
}

bool FrameDebugSystem::CanSubmitToChannel(DebugChannelID _id) const
{
	if (!_id.IsValid())
	{
		DEBUG_LOG_ERROR("IDが不正です\n");
		return false;
	}

	const DebugChannelData* channel = FindChannel(_id);

	if (!channel)
	{
		return false;
	}

	return channel->enabled;
}
