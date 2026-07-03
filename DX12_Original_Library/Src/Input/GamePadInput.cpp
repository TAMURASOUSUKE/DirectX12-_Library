#include "GamePadInput.h"
#include <windows.h>
#include <algorithm>
#include "InputConstant.h"
#include "InputName.h"
#pragma comment(lib, "Xinput.lib")

static_assert(static_cast<WORD>(PadCode::Button::A) == XINPUT_GAMEPAD_A,
	"PadCode::A がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::B) == XINPUT_GAMEPAD_B,
	"PadCode::B がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::X) == XINPUT_GAMEPAD_X,
	"PadCode::X がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::Y) == XINPUT_GAMEPAD_Y,
	"PadCode::Y がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::BACK) == XINPUT_GAMEPAD_BACK,
	"PadCode::BACK がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::START) == XINPUT_GAMEPAD_START,
	"PadCode::START がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::DPAD_DOWN) == XINPUT_GAMEPAD_DPAD_DOWN,
	"PadCode::DPAD_DOWN がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::DPAD_UP) == XINPUT_GAMEPAD_DPAD_UP,
	"PadCode::DPAD_UP がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::DPAD_LEFT) == XINPUT_GAMEPAD_DPAD_LEFT,
	"PadCode::DPAD_LEFT がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::DPAD_RIGHT) == XINPUT_GAMEPAD_DPAD_RIGHT,
	"PadCode::DPAD_RIGHT がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::LEFT_THUMB) == XINPUT_GAMEPAD_LEFT_THUMB,
	"PadCode::LEFT_THUMB がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::RIGHT_THUMB) == XINPUT_GAMEPAD_RIGHT_THUMB,
	"PadCode::RIGHT_THUMB がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button:: LEFT_SHOULDER) == XINPUT_GAMEPAD_LEFT_SHOULDER,
	"PadCode::LEFT_SHOULDER がSDKと不一致");
static_assert(static_cast<WORD>(PadCode::Button::RIGHT_SHOULDER) == XINPUT_GAMEPAD_RIGHT_SHOULDER,
	"PadCode::RIGHT_SHOULDER がSDKと不一致");
static_assert(LEFT_STICK_DEADZONE == XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE,
	"左スティックデッドゾーン がSDKと不一致");
static_assert(RIGHT_STICK_DEADZONE == XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE,
	"右スティックデッドゾーン がSDKと不一致");
void GamePadInput::Update()
{
	prevPad = currentPad; // 状態保存
	XINPUT_STATE state{};
	DWORD result{ XInputGetState(0, &state) };
	if (ERROR_SUCCESS == result)
	{
		currentPad = state.Gamepad;
	}
	else
	{
		currentPad = {}; // 入力は残さない
	}
}

// 押されている間(1フレーム目からみる)
bool GamePadInput::IsPress(int _key)
{
	return (currentPad.wButtons & _key);
}

// 押した瞬間
bool GamePadInput::IsPushed(int _key)
{
	return (currentPad.wButtons & _key) && !(prevPad.wButtons & _key);
}

// 離した瞬間
bool GamePadInput::IsReleased(int _key)
{
	return !(currentPad.wButtons & _key) && (prevPad.wButtons & _key);
}

Vector2 GamePadInput::GetStickValue(PadCode::Stick _stick, bool _isInverseY)
{
	// 必要なパラメータ
	short x{ (_stick == PadCode::Stick::LeftStick) ? currentPad.sThumbLX : currentPad.sThumbRX };
	short y{ (_stick == PadCode::Stick::LeftStick) ? currentPad.sThumbLY : currentPad.sThumbRY };
	float deadZone{ (_stick == PadCode::Stick::LeftStick) ? LEFT_STICK_DEADZONE : RIGHT_STICK_DEADZONE };
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
		rate = std::clamp(rate, 0.0f, 1.0f); // 0-1の範囲に収まるようにする
		raw.Normalize();
		return Vector2{raw.x, (_isInverseY) ? raw.y : -raw.y} * rate;
	}
	return Vector2::Zero; // 長さがデッドゾーンを超えていなかったら0
}