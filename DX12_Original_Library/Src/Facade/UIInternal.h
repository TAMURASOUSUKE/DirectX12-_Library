#pragma once
#include "../Math/Vector/Vector2Int.h"

// UIに関するユーザーに公開しない内部関数を定義する
namespace UIInternal
{
	// UIが1フレームの更新に必要とする外部情報
	struct FrameContext
	{
		float unscaledDeltaTime{ 0.0f };
		Vector2Int clientSize{ Vector2Int::Zero };
		Vector2Int virtualSize{ Vector2Int::Zero };
	};

	// 初期化
	void Initialize();
	// 終了処理
	void Finish();
	// フレーム最初の処理
	void BeginFrame(const FrameContext& _context);
}
