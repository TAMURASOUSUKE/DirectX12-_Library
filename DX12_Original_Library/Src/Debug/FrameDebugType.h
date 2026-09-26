#pragma once
#include <string>
#include <cstdint>
#include <vector>

// 内部の処理情報を管理する際のDebugSystemで共有する軽量な型など

// どの分類かを表す
struct DebugChannelID
{
	std::uint32_t value{ 0 };
};

// 数値として観測することのできるデータ
struct DebugMetricID
{
	std::uint32_t value{ 0 };
};

// Metricとしての単位
enum class DebugMetrictUint : std::uint32_t
{
	None, // 単位なし
	Count, // 個数
	Byte, // バイト数
	Precent, // 割合
	Distance, // 距離
	Speed, // 速度
	Frequency, // 回 / 秒
};

// データを集約させるときの分類
enum class DebugMetricAggregation
{
	Set, // 値を設定
	Add, // 値を追加
	Max, // 最大値
};

// 登録時から変わらない情報を持つ
struct DeubugMetricDescriptor
{
	std::string name{}; // 表示名
	DebugChannelID channelID{}; // どの分類か
	DebugMetrictUint uint{}; // 単位はどれか
	DebugMetricAggregation aggregation{}; // 登録する際の方法はなにか
};

// 毎フレーム変化する情報を持つ
struct DebugMetricValue
{
	double value{ 0.0 }; // 現在の数値
	bool written{ false }; // このフレームで一度でも更新されたか
};

// フレーム間のデータを集める
struct DebugFrameData
{
	std::vector<DebugMetricValue> metrics{}; // 数値として観測することのできるデータの集まり
};
