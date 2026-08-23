#pragma once

// UIとして表示するボタンの表示状態を定義
enum class UIButtonVisualState
{
	Normal, // 通常状態
	Hovered, // 選択状態
	Pressed, // 押した状態
	Disabled, // 操作不能の状態
};

// UIとして表示するボタンのゲーム内の状態を定義
struct UIButtonState
{
	bool isTarget{ false }; // 現在操作対象か
	bool isPushed{ false }; // このボタン上で押した瞬間か
	bool isHeld{ false }; // このボタン上で始まった入力を押している間か
	bool isReleased{ false }; // このボタン上で始まった入力を離した瞬間か
	bool isActivated{ false }; // 対象中に離してボタン操作が成立したか
	bool isCanceled{ false }; // 対象外で離してボタン操作を中止したか
};
