#pragma once
// 時間に関係する機能を提供する

namespace Time
{
	// 前フレームからの経過時間を秒単位で取得する
	float DeltaTime();
	// 固定更新1回文の時間を単位で取得する
	float FixedDeltaTime();
	// 現在計測されているFPSを取得する
	float FPS();
	// 固定更新後に残った時間の割合を取得する
	float Alpha();
	// 固定更新を実行する必要があるか
	bool IsFixedUpdateRequired();
	// 固定更新一回分の時間を消費する
	void ConsumeFixedTime();
	// FPSの上限値を設定する(0を指定するとFPS制限値を解除する)
	void SetTargetFPS(int _targetFPS = 60);
	// 現在設定されているFPS上限を取得する
	int GetTargetFPS();
}