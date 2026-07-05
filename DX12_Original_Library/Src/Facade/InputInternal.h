#pragma once
#include <windows.h>
// ユーザーが触れない入力関数を宣言する
namespace InputInternal
{
	bool Initialize(HWND _hwnd); // 初期化
	void BeginFrame(); // フレーム開始処理
	void EndFrame(); // フレーム終了処理
	void Finish(); // 終了処理

	// wheelが回された分だけ加算して積む : コールバック用関数
	void AddMouseWheelDelta(short _delta);
}