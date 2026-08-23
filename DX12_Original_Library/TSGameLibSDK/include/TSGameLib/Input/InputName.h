#pragma once
#include <cstdint>
#include <variant>

namespace KeyCode
{
	// Windowsの仮想キーをスコープ付き列挙(enum class)で包みユーザーが認識しやすくする
	// KeyCodeとPadCodeの取り違えはコンパイルエラーになる（型安全）
	// 基底型BYTE = 0〜255のキーボード添字の世界
	// キーボード
	enum class Button : std::uint8_t
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
		SPACE = 0x20, // スペース
		LCTRL = 0xA2, // 左CTRL
		RCTRL = 0xA3, // 右CTRL
		CTRL = 0x11, // 両対応CTRL
		LSHIFT = 0xA0, // 左シフト
		RSHIFT = 0xA1, // 右シフト
		SHIFT = 0x10, // 両対応シフト
		LALT = 0xA4, // 左ALT
		RALT = 0xA5, // 右ALT
		ALT = 0x12, // 両対応ALT
		TAB = 0x09, // タブ
		RETURN = 0x0D, // エンター
		ESC = 0x1B, // ESC

		// 矢印
		RIGHT = 0x27, // 右
		LEFT = 0x25, // 左
		UP = 0x26, // 上
		DOWN = 0x28, // 下

	};
}


// XInputのボタンマスク
// 基底型WORD = XINPUT_GAMEPAD::wButtonsと同じ16bitマスクの世界
// 値はXinput.hのXINPUT_GAMEPAD_*と同値の直書き（公開ヘッダーからXInput依存を消すため）
// 数値の正しさはGamePadInput.cpp側のstatic_assertで照合する
namespace PadCode
{
	// ボタン類
	enum class Button : std::uint16_t
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

// マウスの入力を判別するための名前空間
// Windowsから受け取るのでVK_...系から取得する
namespace MouseCode
{
	// クリック類
	enum class Click : std::uint8_t
	{
		LEFT = 0x01,  // 左クリック
		RIGHT = 0x02, // 右クリック
		MIDDLE = 0x04, // 真ん中
		SIDE01 = 0x05, // サイドボタン01
		SIDE02 = 0x06, // サイドボタン02
	};
}

using Binding = std::variant<KeyCode::Button, PadCode::Button, PadCode::Trigger, MouseCode::Click>; // ボタン系の抽象化に使う種類をまとめる

// 操作方式を表す
enum class InputMethod
{
	KeyboardMouse,
	GamePad,
};
