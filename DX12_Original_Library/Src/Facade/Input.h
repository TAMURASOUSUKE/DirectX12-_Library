#pragma once
#include "../Input/InputName.h"

// 入力に関する機能をユーザーに提供する
namespace Input
{
	// 抽象化 : 押している間
	bool IsPress(int _key); 
	// 抽象化 : 押した瞬間
	bool IsPushed(int _key);
	// 抽象化 : 離した瞬間
	bool IsReleased(int _key);

	 // キーボード : 押している間
	bool IsKeyPress(KeyCode _key);
	// キーボード : 押した瞬間
	bool IsKeyPushed(KeyCode _key);
	// キーボード : 離した瞬間
	bool IsKeyReleased(KeyCode _key);

	
	 // ゲームパッド : 押している間
	bool IsPadPress(PadCode _key);
	// ゲームパッド : 押した瞬間
	bool IsPadPushed(PadCode _key); 
	// ゲームパッド : 離した瞬間
	bool IsPadReleased(PadCode _key);
}