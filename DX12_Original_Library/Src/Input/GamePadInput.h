#pragma once
#include <windows.h>
#include <XInput.h>
#include "InputName.h"
#include "../Math/TSMath.h"
#include "InputBase.h"


// コントローラーからの入力を受け取る(現在はXboxのみで今後Switch,PSへ拡張)
class GamePadInput : public InputBase
{
public:
	void Update() override; // 入力更新

	bool IsPress(int _key) override; // 押している間
	bool IsPress(PadCode::Trigger _trigger); // トリガー用
	bool IsPushed(int _key) override; // 押した瞬間
	bool IsPushed(PadCode::Trigger _trigger); // トリガー用
	bool IsReleased(int _key) override; // 離した瞬間
	bool IsReleased(PadCode::Trigger _trigger); // トリガー用

	// 選択したトリガーがどれだけ押されているかを0-1で返す(完全に押されていれば1)
	float GetTriggerValue(PadCode::Trigger _trigger);
	
	// 引数に入れた方のスティックの値を取得する(boolでY軸反転を行うか判断する)
	Vector2 GetStickValue(PadCode::Stick _stick, bool _isInverseY);
private:
	// スティックから出力された値デッドゾーンを適用して-1～1に正規化する関数(boolはY軸反転を行うかどうか)
	Vector2 ApplyNormalizeAndDeadZone(short _x, short _y, float _deadZone, bool _isInverseY);
	// 上記関数のtrigger版オーバーロード
	float ApplyNormalizeAndDeadZone(float _value, float _threshold);
private:
	XINPUT_GAMEPAD currentPad{}; // 現在の入力
	XINPUT_GAMEPAD prevPad{}; // 1フレーム前の入力

	// トリガーがボタンとしてオンだったかを見る変数
	// 今フレーム
	bool isLeftTriggerPressed{ false };
	bool isRightTriggerPressed{ false };
	// 前フレーム
	bool isLeftTriggerPrevPressed{ false };
	bool isRightTriggerPrevPressed{ false };
};
