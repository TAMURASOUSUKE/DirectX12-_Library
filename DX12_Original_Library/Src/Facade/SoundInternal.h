#pragma once
#include <windows.h>
// ユーザーが触れない音関連の関数を宣言する
namespace SoundInternal
{
	bool Initialize(HWND _hwnd); // 初期化
	void BeginFrame(); // フレーム開始処理
	void EndFrame(); // フレーム終了処理
	void Finish(); // 終了処理
}