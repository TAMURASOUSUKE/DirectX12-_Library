#pragma once
// 外部に公開しない時間を制御する関数群

namespace TimeInternal
{
	// 最初の処理
	void Initialize();
	// フレームでの最初の処理
	void BeginFrame();
	// フレームでの最後の処理
	void EndFrame();
	// 最後の処理
	void Finish();
	// VSyncの有効無効を設定
	void SetVSync(bool _isVSyncEnabled);
}