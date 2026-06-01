#pragma once

// Windowsの仮想キーを名前空間で囲みユーザーが認識しやすいようにする
#include <windows.h>

// バイト型で定義する
namespace KeyCode
{
	// アルファベットキー (A-Z)
	constexpr BYTE A{ 'A' };
	constexpr BYTE B{ 'B' };
	constexpr BYTE C{ 'C' };
	constexpr BYTE D{ 'D' };
	constexpr BYTE E{ 'E' };
	constexpr BYTE F{ 'F' };
	constexpr BYTE G{ 'G' };
	constexpr BYTE H{ 'H' };
	constexpr BYTE I{ 'I' };
	constexpr BYTE J{ 'J' };
	constexpr BYTE K{ 'K' };
	constexpr BYTE L{ 'L' };
	constexpr BYTE M{ 'M' };
	constexpr BYTE N{ 'N' };
	constexpr BYTE O{ 'O' };
	constexpr BYTE P{ 'P' };
	constexpr BYTE Q{ 'Q' };
	constexpr BYTE R{ 'R' };
	constexpr BYTE S{ 'S' };
	constexpr BYTE T{ 'T' };
	constexpr BYTE U{ 'U' };
	constexpr BYTE V{ 'V' };
	constexpr BYTE W{ 'W' };
	constexpr BYTE X{ 'X' };
	constexpr BYTE Y{ 'Y' };
	constexpr BYTE Z{ 'Z' };

	// 数字キー(D = Dgit)
	constexpr BYTE D0{ '0' };
	constexpr BYTE D1{ '1' };
	constexpr BYTE D2{ '2' };
	constexpr BYTE D3{ '3' };
	constexpr BYTE D4{ '4' };
	constexpr BYTE D5{ '5' };
	constexpr BYTE D6{ '6' };
	constexpr BYTE D7{ '7' };
	constexpr BYTE D8{ '8' };
	constexpr BYTE D9{ '9' };

	// 特殊キー
	constexpr BYTE SPACE{ VK_SPACE }; // スペース
	constexpr BYTE LCTRL{ VK_LCONTROL }; // 左CTRL
	constexpr BYTE RCTRL{ VK_RCONTROL }; // 右CTRL
	constexpr BYTE CTRL{ VK_CONTROL }; // 両対応CTRL
	constexpr BYTE LSHIFT{ VK_LSHIFT }; // 左シフト
	constexpr BYTE RSHIFT{ VK_RSHIFT }; // 右シフト
	constexpr BYTE SHIFT{ VK_SHIFT }; // 両対応シフト
	constexpr BYTE LALT{ VK_LMENU }; // 左ALT
	constexpr BYTE RALT{ VK_RMENU }; // 右ALT
	constexpr BYTE ALT{ VK_MENU }; // 両対応ALT
	constexpr BYTE TAB{ VK_TAB }; // タブ
	constexpr BYTE RETURN{ VK_RETURN }; // エンター
	constexpr BYTE ESC{ VK_ESCAPE }; // ESC
	
	// 矢印
	constexpr BYTE RIGHT{ VK_RIGHT }; // 右
	constexpr BYTE LEFT{ VK_LEFT }; // 左
	constexpr BYTE UP{ VK_UP }; // 上
	constexpr BYTE DOWN{ VK_DOWN }; // 下
}
