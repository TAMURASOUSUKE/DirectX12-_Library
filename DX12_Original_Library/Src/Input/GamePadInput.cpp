#include "GamePadInput.h"
#include <windows.h>
#include <algorithm>
#include "InputConstant.h"
#include "InputName.h"
#pragma comment(lib, "Xinput.lib")

// 公開ヘッダーに直接書いたパッドーコードがXInputと一致しているかをコンパイル時に確認する
static_assert(static_cast<WORD>(PadCode::Button::A) == XINPUT_GAMEPAD_A, "PadCode::A がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::B) == XINPUT_GAMEPAD_B, "PadCode::B がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::X) == XINPUT_GAMEPAD_X, "PadCode::X がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::Y) == XINPUT_GAMEPAD_Y, "PadCode::Y がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::BACK) == XINPUT_GAMEPAD_BACK, "PadCode::BACK がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::START) == XINPUT_GAMEPAD_START, "PadCode::START がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::DOWN) == XINPUT_GAMEPAD_DPAD_DOWN, "PadCode::DOWN がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::UP) == XINPUT_GAMEPAD_DPAD_UP, "PadCode::UP がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::LEFT) == XINPUT_GAMEPAD_DPAD_LEFT, "PadCode::LEFT がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::RIGHT) == XINPUT_GAMEPAD_DPAD_RIGHT, "PadCode::RIGHT がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::LEFT_THUMB) == XINPUT_GAMEPAD_LEFT_THUMB, "PadCode::LEFT_THUMB がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::RIGHT_THUMB) == XINPUT_GAMEPAD_RIGHT_THUMB, "PadCode::RIGHT_THUMB がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button:: LEFT_SHOULDER) == XINPUT_GAMEPAD_LEFT_SHOULDER, "PadCode::LEFT_SHOULDER がSDKと不一致\n");
static_assert(static_cast<WORD>(PadCode::Button::RIGHT_SHOULDER) == XINPUT_GAMEPAD_RIGHT_SHOULDER, "PadCode::RIGHT_SHOULDER がSDKと不一致\n");
static_assert(LEFT_STICK_DEADZONE == XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE, "左スティックデッドゾーン がSDKと不一致\n");
static_assert(RIGHT_STICK_DEADZONE == XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE, "右スティックデッドゾーン がSDKと不一致\n");
static_assert(TRIGGER_THRESHOLD == XINPUT_GAMEPAD_TRIGGER_THRESHOLD, "トリガーの閾値がSDKと不一致\n");
void GamePadInput::Update()
{
	prevPad = currentPad; // 状態保存

	// 更新前の前フレームの最終的な判定結果の保存
	isLeftTriggerPrevPressed = isLeftTriggerPressed;
	isRightTriggerPrevPressed = isRightTriggerPressed;

	XINPUT_STATE state{};
	DWORD result{ XInputGetState(0, &state) };
	if (ERROR_SUCCESS == result)
	{
		currentPad = state.Gamepad;

		// ヒステリシス状態の更新処理(押されていたかをみて閾値を動的に変更する)
		// 左トリガー
		float leftVal{ static_cast<float>(currentPad.bLeftTrigger) };
		float leftThreshold{ (isLeftTriggerPressed) ? TRIGGER_RELEASE_THRESHOLD : TRIGGER_THRESHOLD };
		isLeftTriggerPressed = (leftVal >= leftThreshold);

		// 右トリガー
		float rightVal{ static_cast<float>(currentPad.bRightTrigger) };
		float rightThreshold{ (isRightTriggerPressed) ? TRIGGER_RELEASE_THRESHOLD : TRIGGER_THRESHOLD };
		isRightTriggerPressed = (rightVal >= rightThreshold);
	}
	else
	{
		currentPad = {}; // 入力は残さない
		isLeftTriggerPressed = false;
		isRightTriggerPressed = false;
	}

}

// 押されている間(1フレーム目からみる)
bool GamePadInput::IsPress(int _key)
{
	return (currentPad.wButtons & _key);
}

bool GamePadInput::IsPress(PadCode::Trigger _trigger)
{
	return (_trigger == PadCode::Trigger::LEFT) ? isLeftTriggerPressed : isRightTriggerPressed;
}

// 押した瞬間
bool GamePadInput::IsPushed(int _key)
{
	return (currentPad.wButtons & _key) && !(prevPad.wButtons & _key);
}

bool GamePadInput::IsPushed(PadCode::Trigger _trigger)
{
	bool current{ (_trigger == PadCode::Trigger::LEFT) ? isLeftTriggerPressed : isRightTriggerPressed };
	bool prev{ (_trigger == PadCode::Trigger::LEFT) ? isLeftTriggerPrevPressed : isRightTriggerPrevPressed };
	return current && !prev;
}

// 離した瞬間
bool GamePadInput::IsReleased(int _key)
{
	return !(currentPad.wButtons & _key) && (prevPad.wButtons & _key);
}

bool GamePadInput::IsReleased(PadCode::Trigger _trigger)
{
	bool current{ (_trigger == PadCode::Trigger::LEFT) ? isLeftTriggerPressed : isRightTriggerPressed };
	bool prev{ (_trigger == PadCode::Trigger::LEFT) ? isLeftTriggerPrevPressed : isRightTriggerPrevPressed };
	return !current && prev;
}

bool GamePadInput::IsInputActiveThisFrame()
{
	// ボタンが一つでも入力されているか、トリガーが入力されているか、スティックが倒されているか
	bool isButtonPushed{ false }; // ボタンが押されているか
	bool isTriggerPushed{ false }; // トリガーが押されているか
	bool isStickMoved{ false }; // スティックを動かしたか

	// ボタンが押されているか
	isButtonPushed = currentPad.wButtons & ~prevPad.wButtons; // 0以外なら押されていると判断できる

	// スティックチェック
	Vector2 leftStickValue{ GetStickValue(PadCode::Stick::LEFT, false) };
	Vector2 rightStickValue{ GetStickValue(PadCode::Stick::RIGHT, false) };
	// 左右どちらかのスティックが閾値より大きく動いていれば操作している
	if (leftStickValue.LengthSquared() > LEFT_STICK_DEADZONE * LEFT_STICK_DEADZONE ||
		rightStickValue.LengthSquared() > RIGHT_STICK_DEADZONE * RIGHT_STICK_DEADZONE)  isStickMoved = true;

	// トリガーチェック
	float leftTriggerValue{ GetTriggerValue(PadCode::Trigger::LEFT) };
	float rightTriggerValue{ GetTriggerValue(PadCode::Trigger::RIGHT) };
	if (leftTriggerValue > TRIGGER_RELEASE_THRESHOLD || rightTriggerValue > TRIGGER_RELEASE_THRESHOLD) isTriggerPushed = true;

	return isButtonPushed || isStickMoved || isTriggerPushed;
}

float GamePadInput::GetTriggerValue(PadCode::Trigger _trigger)
{
	float result{ (_trigger == PadCode::Trigger::LEFT) ? static_cast<float>(currentPad.bLeftTrigger) : static_cast<float>(currentPad.bRightTrigger) };
	return ApplyNormalizeAndDeadZone(result, TRIGGER_THRESHOLD);
}

Vector2 GamePadInput::GetStickValue(PadCode::Stick _stick, bool _isInverseY)
{
	// 必要なパラメータ
	short x{ (_stick == PadCode::Stick::LEFT) ? currentPad.sThumbLX : currentPad.sThumbRX };
	short y{ (_stick == PadCode::Stick::LEFT) ? currentPad.sThumbLY : currentPad.sThumbRY };
	float deadZone{ (_stick == PadCode::Stick::LEFT) ? LEFT_STICK_DEADZONE : RIGHT_STICK_DEADZONE };
	return Vector2{ ApplyNormalizeAndDeadZone(x, y, deadZone, _isInverseY) };
}

Vector2 GamePadInput::ApplyNormalizeAndDeadZone(short _x, short _y, float _deadZone ,bool _isInverseY)
{
	// 入力された値でベクトルを作る
	Vector2 raw{ static_cast<float>(_x), static_cast<float>(_y) };
	float length{ raw.Length() };
	if (length > _deadZone)
	{
		float rate{ Math::InverseLerp(_deadZone, MAX_STICK_VALUE, length) }; // 補間率
		// スティックを倒したときの長さを基準に補間率を導出している
		// MaxStickValueはスティックを倒したときの軸の最大値のため、角に倒すと長さは最大値を超えるためclampする必要がある
		rate = std::clamp(rate, 0.0f, 1.0f); // 0-1の範囲に収まるようにする
		raw.Normalize();
		return Vector2{raw.x, (_isInverseY) ? raw.y : -raw.y} * rate;
	}
	return Vector2::Zero; // 長さがデッドゾーンを超えていなかったら0
}

float GamePadInput::ApplyNormalizeAndDeadZone(float _value, float _threshold)
{
	// 入力された値を得る
	if (_value >= _threshold)
	{
		// トリガーの値は0-255なので最大値255で割れば0-1に制限できる
		return (Math::InverseLerp(_threshold, MAX_TRIGGER_VALUE, _value)); 
	}
	return 0.0f;
}
