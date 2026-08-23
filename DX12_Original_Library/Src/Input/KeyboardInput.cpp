#include "InputName.h"
#include "InputConstant.h"
#include "KeyboardInput.h"

// 公開ヘッダーに直接書いたキーコードがWin32のVirtual-Key Codeと一致しているかをコンパイル時に確認する
static_assert(static_cast<int>(KeyCode::Button::SPACE) == VK_SPACE, "KeyCode::Button::SPACEがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::LCTRL) == VK_LCONTROL, "KeyCode::Button::LCTRLがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::RCTRL) == VK_RCONTROL, "KeyCode::Button::RCTRLがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::CTRL) == VK_CONTROL, "KeyCode::Button::CTRLがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::LSHIFT) == VK_LSHIFT, "KeyCode::Button::LSHIFTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::RSHIFT) == VK_RSHIFT, "KeyCode::Button::RSHIFTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::SHIFT) == VK_SHIFT, "KeyCode::Button::SHIFTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::LALT) == VK_LMENU, "KeyCode::Button::LALTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::RALT) == VK_RMENU, "KeyCode::Button::RALTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::ALT) == VK_MENU, "KeyCode::Button::ALTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::TAB) == VK_TAB, "KeyCode::Button::TABがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::RETURN) == VK_RETURN, "KeyCode::Button::RETURNがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::ESC) == VK_ESCAPE, "KeyCode::Button::ESCがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::LEFT) == VK_LEFT, "KeyCode::Button::LEFTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::UP) == VK_UP, "KeyCode::Button::UPがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::RIGHT) == VK_RIGHT, "KeyCode::Button::RIGHTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(KeyCode::Button::DOWN) == VK_DOWN, "KeyCode::Button::DOWNがVirtualKeyCodeと不一致\n");

// 更新
void KeyboardInput::Update()
{
	memcpy(prevKeys, currentKeys, 256);
	BOOL result{ GetKeyboardState(currentKeys) };
	if (!result) memset(currentKeys, 0, 256); // 0リセットで入力を残さない
}

// 今のフレームで押しているなら
bool KeyboardInput::IsPress(int _key)
{
	return(currentKeys[_key] & MOST_SIGNIFICANT_BIT);
}

// 前のフレームでは押されておらず今のフレームで押しているなら
bool KeyboardInput::IsPushed(int _key)
{
	return (currentKeys[_key] & MOST_SIGNIFICANT_BIT) && !(prevKeys[_key] & MOST_SIGNIFICANT_BIT);
}

// 前のフレームで押していて今のフレームで押していないなら
bool KeyboardInput::IsReleased(int _key)
{
	 return!(currentKeys[_key] & MOST_SIGNIFICANT_BIT) && (prevKeys[_key] & MOST_SIGNIFICANT_BIT);
}

// このフレーム操作が発生したか
bool KeyboardInput::IsInputActiveThisFrame()
{
	// 今のフレームで一つでも押されているかをチェック
	for (int keyIndex = 0; keyIndex < 256; keyIndex++)
	{
		// 押し続けているか判定すると入力方法の切り替えが曖昧になるので瞬間で判定する
		if (IsPushed(keyIndex)) return true;
	}
	return false;
}
