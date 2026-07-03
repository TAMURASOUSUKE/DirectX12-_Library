#pragma once
#include <windows.h> // VK_*（仮想キーコード）と BYTE/WORD のため

namespace KeyCode
{
	// Windowsの仮想キーをスコープ付き列挙(enum class)で包みユーザーが認識しやすくする
	// KeyCodeとPadCodeの取り違えはコンパイルエラーになる（型安全）
	// 基底型BYTE = 0〜255のキーボード添字の世界
	// キーボードやクリック類
	enum class Button : BYTE
	{
		// アルファベットキー (A-Z)
		A = 'A',
		B = 'B',
		C = 'C',
		D = 'D',
		E = 'E',
		F = 'F',
		G = 'G',
		H = 'H',
		I = 'I',
		J = 'J',
		K = 'K',
		L = 'L',
		M = 'M',
		N = 'N',
		O = 'O',
		P = 'P',
		Q = 'Q',
		R = 'R',
		S = 'S',
		T = 'T',
		U = 'U',
		V = 'V',
		W = 'W',
		X = 'X',
		Y = 'Y',
		Z = 'Z',

		// 数字キー(D = Digit)
		D0 = '0',
		D1 = '1',
		D2 = '2',
		D3 = '3',
		D4 = '4',
		D5 = '5',
		D6 = '6',
		D7 = '7',
		D8 = '8',
		D9 = '9',

		// 特殊キー
		SPACE = VK_SPACE, // スペース
		LCTRL = VK_LCONTROL, // 左CTRL
		RCTRL = VK_RCONTROL, // 右CTRL
		CTRL = VK_CONTROL, // 両対応CTRL
		LSHIFT = VK_LSHIFT, // 左シフト
		RSHIFT = VK_RSHIFT, // 右シフト
		SHIFT = VK_SHIFT, // 両対応シフト
		LALT = VK_LMENU, // 左ALT
		RALT = VK_RMENU, // 右ALT
		ALT = VK_MENU, // 両対応ALT
		TAB = VK_TAB, // タブ
		RETURN = VK_RETURN, // エンター
		ESC = VK_ESCAPE, // ESC

		// 矢印
		RIGHT = VK_RIGHT, // 右
		LEFT = VK_LEFT, // 左
		UP = VK_UP, // 上
		DOWN = VK_DOWN, // 下

		// クリック
	};

	// マウス類
	enum class Mouse
	{
	};
}


// XInputのボタンマスク
// 基底型WORD = XINPUT_GAMEPAD::wButtonsと同じ16bitマスクの世界
// 値はXinput.hのXINPUT_GAMEPAD_*と同値の直書き（公開ヘッダーからXInput依存を消すため）
// 数値の正しさはGamePadInput.cpp側のstatic_assertで照合する
namespace PadCode
{
	// ボタン類
	enum class Button : WORD
	{
		UP = 0x0001, // 十字キー上
		DOWN = 0x0002, // 十字キー下
		LEFT = 0x0004, // 十字キー左
		RIGHT = 0x0008, // 十字キー右
		START = 0x0010, // スタート
		BACK = 0x0020, // バック
		LEFT_THUMB = 0x0040, // 左スティック押し込み
		RIGHT_THUMB = 0x0080, // 右スティック押し込み
		LEFT_SHOULDER = 0x0100, // LB
		RIGHT_SHOULDER = 0x0200, // RB
		A = 0x1000, // Aボタン
		B = 0x2000, // Bボタン
		X = 0x4000, // Xボタン
		Y = 0x8000, // Yボタン
	};

	// スティック類
	enum class Stick
	{
		LEFT, // 左スティック
		RIGHT, // 右スティック
	};

	// 左右トリガー
	enum class Trigger
	{
		LEFT, // 左トリガー
		RIGHT, // 右トリガー
	};
}
