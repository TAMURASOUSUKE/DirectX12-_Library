#pragma once
#include <limits>
#include <string>
#include <cstdint>
#include <vector>
#include "../Math/Vector/Vector3.h"
#include "../Math/Vector/Vector4.h"
#include "../Math/Quaternion/Quaternion.h"

// 内部の処理情報を管理する際のDebugSystemで共有する軽量な型など

class FrameDebugSystem;

// どの分類かを表す
class DebugChannelID
{
public:
	DebugChannelID() = default;

	// 外部からは取得しかできない
	bool IsValid() const { return value != InvalidValue; }

	bool operator==(DebugChannelID _other) const { return value == _other.value; }

	bool operator!=(DebugChannelID _other) const { return value != _other.value; }

private:
	explicit DebugChannelID(std::uint32_t _value) : value{ _value } {}
	static constexpr std::uint32_t InvalidValue{ (std::numeric_limits<std::uint32_t>::max)() };

	// 実際の内部の値
	std::uint32_t value{ InvalidValue };

	// このクラスからはIDを作れるようにする
	friend class FrameDebugSystem;
};

// 数値として観測することのできるデータ
class DebugMetricID
{
public:
	DebugMetricID() = default;

	// 外部からは取得しかできない
	bool IsValid() const { return value != InvalidValue; }

	bool operator==(DebugMetricID _other) const { return value == _other.value; }

	bool operator!=(DebugMetricID _other) const { return value != _other.value; }

private:
	explicit DebugMetricID(std::uint32_t _value) : value{ _value } {}
	static constexpr std::uint32_t InvalidValue{ (std::numeric_limits<std::uint32_t>::max)() };

	// 実際の内部の値
	std::uint32_t value{ InvalidValue };

	// このクラスからはIDを作れるようにする
	friend class FrameDebugSystem;
};

// Metricとしての単位
enum class DebugMetricUnit : std::uint8_t
{
	None, // 単位なし
	Count, // 個数
	Bytes, // バイト数
	Percent, // 割合
	Distance, // 距離
	Speed, // 速度
	Frequency, // 回 / 秒
	Seconds, // 秒
	Milliseconds, // ミリ秒
};

// 同一フレーム内の集約情報
enum class DebugMetricAggregation : std::uint8_t
{
	Set, // 値を設定(最後に提出された値が採用)
	Add, // 値を追加
	Max, // 最大値
};

// MetricをOverlayへどのように表示するか
enum class DebugMetricDisplayMode : std::uint8_t
{
	Current, // 完成した1フレームの値だけを表示
	WindowStatistics, // 一定時間の更新、平均、最大を表示
};

// 登録時から変わらない情報を持つ
struct DebugMetricDescriptor
{
	std::string name{}; // 表示名
	DebugChannelID channelID{}; // どの分類か
	DebugMetricUnit unit{}; // 単位はどれか
	DebugMetricAggregation aggregation{}; // 登録する際の方法はなにか
	DebugMetricDisplayMode displayMode{ DebugMetricDisplayMode::Current }; // 時間方向にどのように表示するか
};

// 毎フレーム変化する情報を持つ
struct DebugMetricValue
{
	double value{ 0.0 }; // 現在の数値
	bool written{ false }; // このフレームで一度でも更新されたか
};

// 線分のデバッグ表示用
struct DebugLineCommand
{
	DebugChannelID channelID{}; // チャンネル
	Vector3 start{ Vector3::Zero }; // 始点
	Vector3 end{ Vector3::Zero }; // 終点
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // 色
};

// 箱のデバッグ表示用
struct DebugBoxCommand
{
	DebugChannelID channelID{}; // チャンネル
	Vector3 center{ Vector3::Zero }; // 中心
	Vector3 halfSize{ Vector3::Zero }; // サイズの半分
	Quaternion rotation{ Quaternion::Identity }; // 回転
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // 色
};

// 球のデバッグ表示用
struct DebugSphereCommand
{
	DebugChannelID channelID{}; // チャンネル
	Vector3 center{ Vector3::Zero }; // 中心
	float radius{ 0.0f }; // 半径
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // 色
};

// カプセルのデバッグ表示用
struct DebugCapsuleCommand
{
	DebugChannelID channelID{}; // チャンネル
	Vector3 start{ Vector3::Zero }; // 始点
	Vector3 end{ Vector3::Zero }; // 終点
	float radius{ 0.0f }; // 半径
	Vector4 color{ 1.0f, 1.0f, 1.0f, 1.0f }; // 色
};

// 1フレーム分のデータを集める
struct DebugFrameData
{
	std::vector<DebugMetricValue> metrics{}; // 数値として観測することのできるデータの集まり
	std::vector<DebugLineCommand> lineCommands{}; // 線分のデバッグデータ
	std::vector<DebugBoxCommand> boxCommands{}; // 箱のデバッグデータ
	std::vector<DebugSphereCommand> sphereCommands{}; // 球のデバッグデータ
	std::vector<DebugCapsuleCommand> capsuleCommands{}; // カプセルのデバッグデータ
};

// 登録される際の分類
struct DebugChannelData
{
	std::string name{}; // Channel名
	bool enabled{ true }; // 現在表示、記録対象になっているかどうか
};

// 一定期間に集計されたMetricの統計結果
struct DebugMetricStatistics
{
	double latest{ 0.0 }; // 区間内で最後に提出された値
	double average{ 0.0 }; // 区間内の平均
	double maximum{ 0.0 }; // 区間内の最大
	std::uint32_t sampleCount{ 0 }; // 区間内で値が提出された回数
	bool valid{ false }; // 有効な統計結果が存在するか
};
