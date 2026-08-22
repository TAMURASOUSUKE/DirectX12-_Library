#pragma once

// UIを操作する際の汎用的な操作をまとめる

// UIの操作方向を定義する
enum class UINavigationDirection
{
	None,
	Up,
	Down,
	Left,
	Right
};

// 長押しもしくは倒しっぱなしの場合のリピート移動(キーリピート方式)の設定
struct UINavigationRepeatSettings
{
	float initialDelay{ 0.35f }; // 最初の連続移動までの待機時間 秒
	float repeatInterval{ 0.1f }; // 連続移動の繰り返し間隔 秒
	float enterThreshold{ 0.65f }; // スティック入力受付閾値
	float releaseThreshold{ 0.3f }; // 中立判定閾値

	// それぞれのパラメータが正しい値を持っているかをチェックする関数
	bool IsValid() const { return initialDelay >= 0.0f && repeatInterval > 0.0f && releaseThreshold >= 0.0f && enterThreshold > releaseThreshold && 1.0f >= enterThreshold; }
};
