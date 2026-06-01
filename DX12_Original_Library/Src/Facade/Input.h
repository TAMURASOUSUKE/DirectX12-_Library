#pragma once

// 入力に関する機能をユーザーに提供する
namespace Input
{
	// 抽象化
	bool IsPress(int _key); // 押している間
	bool IsPushed(int _key); // 押した瞬間
	bool IsReleased(int _key); // 離した瞬間

	// キーボード限定
	bool IsKeyPress(int _key); // 押している間
	bool IsKeyPushed(int _key); // 押した瞬間
	bool IsKeyReleased(int _key); // 離した瞬間

	// パッド限定
	bool IsPadPress(int _key); // 押している間
	bool IsPadPushed(int _key); // 押した瞬間
	bool IsPadReleased(int _key); // 離した瞬間
}