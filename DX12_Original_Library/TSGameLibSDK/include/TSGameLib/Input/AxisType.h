#pragma once
#include <variant>
#include "InputName.h"
#include "../Math/Vector/Vector2.h"

// Axisが最終的に返す値の性質
enum class AxisMode
{
	Value, // UIや移動などに使う 押している量やStickの傾きなど
	Delta, // 主にカメラなど そのフレームで使う変化量を表す
};

// キーボード・Padボタン・マウスボタンのいずれか(Triggerは0-1の中間値があるため含めない)
using AxisDigitalSource = std::variant<KeyCode::Button, PadCode::Button, MouseCode::Click>;

// 押されたらDirection方向へ値を加えるBinding
struct DigitalAxisBinding
{
	AxisDigitalSource source{ KeyCode::Button::W }; // デフォルトをW
	Vector2 direction{ Vector2::Zero }; // この入力が担当する方向
	float scale{ 1.0f }; // 強さ
};

// 左右のスティックから情報をとり倍率を加えるBinding Y反転も対応
struct StickAxisBinding
{
	PadCode::Stick stick{ PadCode::Stick::LEFT }; // 左右のスティック(デフォルトは左)
	float scale{ 1.0f }; // 倍率
	bool isYInverted{ false }; // Y反転するか
};

// 左右のtriggerの値から情報を取り力を加える方向、倍率を加えるBinding
struct TriggerAxisBinding
{
	PadCode::Trigger trigger{ PadCode::Trigger::LEFT }; // 左右のトリガー(デフォルトは左)
	Vector2 direction{ Vector2::Zero }; // 方向
	float scale{ 1.0f }; // 倍率
};

// マウスの1フレームの移動量から倍率を加えるBinding Y反転対応
struct MouseDeltaAxisBinding
{
	float scale{ 1.0f }; // 倍率
	bool isYInverted{ false }; // Y軸反転
};

// 各Bindingのvariantを作り登録できる形式にする
using AxisBinding = std::variant<DigitalAxisBinding, StickAxisBinding, TriggerAxisBinding, MouseDeltaAxisBinding>;
