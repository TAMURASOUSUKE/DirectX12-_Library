#pragma once
#include "../Debug/FrameDebugOverlayType.h"

// ユーザーに提供するマクロ以外のデバッグでユーザーに公開しない部分の機能を担当する
namespace DebugInternal
{
	// 初期化
	bool Initialize();
	// フレームの最初の処理
	void BeginFrame();
	// フレームの最後の処理
	void EndFrame();
	// 終了処理
	void Finish();

	// 完成済みのDebug描画命令を取得する
	const DebugOverlayFrame& GetOverlayFrame();
}
