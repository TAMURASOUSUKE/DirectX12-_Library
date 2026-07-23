#pragma once
#include <windows.h>
// ユーザーが触れない音関連の関数を宣言する
namespace SoundInternal
{
	bool Initialize(); // 初期化
	void BeginFrame(float _deltaTime); // フレーム開始処理(クロスフェード更新用にdeltaTimeが必要)
	void EndFrame(); // フレーム終了処理
	void Finish(); // 終了処理
}