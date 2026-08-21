#pragma once
#include <functional>
#include "../Input/InputName.h"
#include "../Core/Handle/UIButtonHandle.h"

// UIに関する機能をユーザーに提供する
namespace UI
{
	using ButtonTargetQuery = std::function<bool()>; // 操作対象か返す関数(引数なし戻り値bool)
	using ButtonEventCallback = std::function<void()>; // 各イベントで呼び出す関数(引数なし戻り値void)

	// 指定キーと操作対象か判定する関数を渡すしてボタンのハンドルを作成
	UIButtonHandle Create(KeyCode::Button _key, ButtonTargetQuery _targetQuery);

	// 指定ボタン操作が成立したときに呼び出す関数を設定
	bool SetOnActivated(UIButtonHandle _handle, ButtonEventCallback _callback);

	// ハンドルを指定してボタンを破棄
	bool DestroyButton(UIButtonHandle _handle);
}
