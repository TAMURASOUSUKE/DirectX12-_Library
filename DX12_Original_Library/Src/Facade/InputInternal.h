#pragma once

// ユーザーが触れない入力関数を宣言する
namespace InputInternal
{
	bool Initialize(); // 初期化
	void BeginFrame(); // フレーム開始処理
	void EndFrame(); // フレーム終了処理
	void Fnish(); // 終了処理
}