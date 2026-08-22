#pragma once
#include <vector>
#include "../Core/Handle/UIButtonHandle.h"
#include "UINavigationType.h"

// 登録されたボタン選択位置を管理する
class UIButtonNavigation
{
public:
	UIButtonNavigation() = default;
	~UIButtonNavigation() = default;

	UIButtonNavigation(const UIButtonNavigation& _other) = delete;
	UIButtonNavigation& operator=(const UIButtonNavigation& _other) = delete;

	// 1行に並べるボタン数を設定して初期化する
	bool Setup(int _columnCount);

	// 全登録を破棄して終了状態へ戻す
	void Shutdown();

	// ナビゲーション対象へボタンを追加する
	bool AddButton(UIButtonHandle _handle);

	// ナビゲーション対象からボタンを削除する
	bool RemoveButton(UIButtonHandle _handle);

	// 登録されているボタンをすべて解除する
	void Clear();

	// 選択位置を最初のボタンへ戻す
	void ResetSelection();

	// 指定方向へ選択を移動する
	bool Move(UINavigationDirection _direction);

	// 指定ボタンを直接選択する
	bool SetSelected(UIButtonHandle _handle);

	// 現在選択されているボタンを取得する
	UIButtonHandle GetSelected() const;

	// 指定ボタンが現在選択されているか
	bool IsSelected(UIButtonHandle _handle) const;

	// 指定ボタンがナビゲーションへ登録されているか
	bool Contains(UIButtonHandle _handle) const;

private:
	std::vector<UIButtonHandle> buttons{}; // ナビゲーション対象
	int selectedIndex{ -1 }; // 選択中のIndex
	int columnCount{ 0 }; // 1行に並べる個数
};
