#pragma once
#include <functional>
#include <utility>
#include  <type_traits>
#include "../Collision/Collider.h" //簡易的な矩形判定ボタンに使用
#include "../Input/InputName.h"
#include "../UI/UINavigationType.h"
#include "../UI/UIButtonType.h"
#include "../Core/Handle/UIButtonHandle.h"

// UIに関する機能をユーザーに提供する
namespace UI
{
	using ButtonTargetQuery = std::function<bool()>; // 操作対象か返す関数(引数なし戻り値bool)
	using ButtonEventCallback = std::function<void()>; // 各イベントで呼び出す関数(引数なし戻り値void)

	// ユーザーが触ってはいけない部分を名前で知らせる
	namespace Detail 
	{
		// 指定抽象化アクションと操作対象かを判定する関数を渡してボタンのハンドルを作成するための内部実装
		UIButtonHandle CreateActionImpl(int _actionIndex, ButtonTargetQuery _targetQuery);
		// 抽象化アクション + 簡易矩形判定を行う関数を設定するボタンのハンドルを作成するための内部実装
		UIButtonHandle CreateActionRectImpl(int _actionIndex, Rect _rect);
		// 抽象化Axisをナビゲーションへ渡すための内部実装
		bool SetNavigationAxisImpl(int _axisIndex);
	}

	// 指定キーと操作対象か判定する関数を渡すしてボタンのハンドルを作成
	UIButtonHandle Create(KeyCode::Button _key, ButtonTargetQuery _targetQuery); // キーボード
	// 指定ボタンと操作対象か判定する関数を渡すしてボタンのハンドルを作成
	UIButtonHandle Create(PadCode::Button _button, ButtonTargetQuery _targetQuery); // ゲームパッド
	// 指定クリックと操作対象か判定する関数を渡すしてボタンのハンドルを作成
	UIButtonHandle Create(MouseCode::Click _click, ButtonTargetQuery _targetQuery); // マウスクリック
	// 指定抽象化アクションと操作対象かを判定する関数を渡してボタンのハンドルを作成
	template<typename TAction>
	UIButtonHandle Create(TAction _action, ButtonTargetQuery _targetQuery)
	{
		static_assert(std::is_enum_v<TAction>, "Actionはenum classで定義してください\n");
		return Detail::CreateActionImpl(static_cast<int>(_action), std::move(_targetQuery));
	}

	// 指定キー + 簡易矩形判定を行うためのRectを渡してボタンのハンドルを作成
	UIButtonHandle Create(KeyCode::Button _key, Rect _hitRect); // キーボード
	// 指定ボタン + 簡易矩形判定を行うためのRectを渡してボタンのハンドルを作成
	UIButtonHandle Create(PadCode::Button _button, Rect _hitRect); // ゲームパッド
	// 指定クリック + 簡易矩形判定を行うためのRectを渡してボタンのハンドルを作成
	UIButtonHandle Create(MouseCode::Click _click, Rect _hitRect); // マウスクリック
	// 指定抽象化アクション + 簡易矩形判定を行うためのRectを渡してボタンのハンドルを作成
	template<typename TAction>
	UIButtonHandle Create(TAction _action, Rect _rect)
	{
		static_assert(std::is_enum_v<TAction>, "Actionはenum classで定義してください\n");
		return Detail::CreateActionRectImpl(static_cast<int>(_action), _rect);
	}

	// キーを指定せず、矩形領域を持つ標準UIボタンを作成する(マウス左クリック・Enter・ゲームパッドAに対応する)
	UIButtonHandle Create(Rect _hitRect);

	// 指定ボタンが選択状態にあるときに呼び出される関数
	bool SetOnTarget(UIButtonHandle _handle, ButtonEventCallback _callback);
	// 指定ボタンが押された瞬間に呼び出す関数を設定
	bool SetOnPushed(UIButtonHandle _handle, ButtonEventCallback _callback);
	// 指定ボタンが押されている時に呼び出す関数を設定
	bool SetOnHeld(UIButtonHandle _handle, ButtonEventCallback _callback);
	// 指定ボタンが離された瞬間に呼び出す関数を設定
	bool SetOnReleased(UIButtonHandle _handle, ButtonEventCallback _callback);
	// 指定ボタン操作が成立したときに呼び出す関数を設定
	bool SetOnActivated(UIButtonHandle _handle, ButtonEventCallback _callback);
	// 指定ボタン操作がキャンセルされたときに呼び出す関数を設定
	bool SetOnCanceled(UIButtonHandle _handle, ButtonEventCallback _callback);

	// ハンドルを指定してボタンを破棄
	bool DestroyButton(UIButtonHandle _handle);

	// 格子状のボタンナビゲーションを初期化する
	bool SetupNavigation(int _columnCount, UINavigationRepeatSettings _setting = {});

	// ナビゲーション対象へボタンを追加する
	bool AddNavigationButton(UIButtonHandle _handle);

	// 指定Axisをナビゲーション登録する
	template<typename TAxis>
	bool SetNavigationAxis(TAxis _axis)
	{
		static_assert(std::is_enum_v<TAxis>, "Axisはenum classで定義してください\n");
		return Detail::SetNavigationAxisImpl(static_cast<int>(_axis));
	}

	// ナビゲーション対象からボタンを外す
	bool RemoveNavigationButton(UIButtonHandle _handle);

	// 選択を最初のボタンへ戻す
	void ResetNavigationSelection();

	// 現在選択されているボタンを取得する
	UIButtonHandle GetSelectedButton();

	// 指定ボタンの現在の表示状態を取得する
	UIButtonVisualState GetVisualState(UIButtonHandle _handle);
}
