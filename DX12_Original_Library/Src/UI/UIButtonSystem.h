#pragma once
#include <vector>
#include <stack>
#include <cstdint>
#include <functional>
#include "UIButton.h"
#include "../Core/Handle/UIButtonHandle.h"

// UIButtonに関するシステムを提供する
class UIButtonSystem
{
public:
	// 各ボタンが自身が操作対象になっているかを返す関数
	using TargetQuery = std::function<bool()>;
	UIButtonSystem() = default; 
	~UIButtonSystem() = default;

	UIButtonSystem(const UIButtonSystem&) = delete;
	UIButtonSystem& operator=(const UIButtonSystem& _other) = delete;

	// 初期設定
	void Setup();
	// 終了処理
	void Shutdown();

	// 登録されている全ての有効なボタンを更新する
	// ナビゲーション操作中ならNavigationTargetだけを操作対象にするそれ以外はTargetQueryを使用する
	void UpdateAll(UIButtonHandle _navigationTarget, bool _useNavigationTarget);

	// ボタンが指定状態の時のイベント登録
	bool SetOnTarget(UIButtonHandle _handle, UIButton::EventCallback _callback);
	// ボタンプッシュ時のイベント登録
	bool SetOnPushed(UIButtonHandle _handle, UIButton::EventCallback _callback);
	// ボタンプレス時のイベント登録
	bool SetOnHeld(UIButtonHandle _handle, UIButton::EventCallback _callback);
	// ボタンリリース時のイベント登録
	bool SetOnReleased(UIButtonHandle _handle, UIButton::EventCallback _callback);
	// ボタン操作成立時のイベントを登録する
	bool SetOnActivated(UIButtonHandle _handle, UIButton::EventCallback _callback);
	// ボタン操作キャンセル時のイベント登録
	bool SetOnCanceled(UIButtonHandle _handle, UIButton::EventCallback _callback);

	// 指定入力からボタンハンドルを作成
	UIButtonHandle Create(UIButtonInputSource _inputSource, TargetQuery _targetQuery);

	// 指定ハンドルの削除
	bool Destroy(UIButtonHandle _handle);

	// 指定ボタンの表示状態を取得する
	UIButtonVisualState GetVisualState(UIButtonHandle _handle);

private:
	// ハンドルの分解(ボタンにはリソースという概念がないためSystem側でlookupするがユーザーに漏れないようにprivate)
	UIButton* Lookup(UIButtonHandle _handle);

	// ボタン一つ分のスロット
	struct UIButtonSlot
	{
		UIButton data{}; // 実データ
		std::uint32_t generation{ 0 }; // 世代
		TargetQuery targetQuery{}; // 自身が操作対象か
		bool isAlive{ false }; // 生存フラグ
	};

private:
	std::vector<UIButtonSlot> slots{}; // ボタンの台帳
	std::stack<int> freeList{}; // ボタンのフリーリスト
};
