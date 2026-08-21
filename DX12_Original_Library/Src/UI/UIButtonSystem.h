#pragma once
#include <vector>
#include <stack>
#include <cstdint>
#include "UIButton.h"
#include "../Core/Handle/UIButtonHandle.h"

// UIButtonに関するシステムを提供する
class UIButtonSystem
{
public:
	UIButtonSystem() = default; 
	~UIButtonSystem() = default;

	UIButtonSystem(const UIButtonSystem&) = delete;
	UIButtonSystem& operator=(const UIButtonSystem& _other) = delete;

	// 初期設定
	void Setup();
	// 終了処理
	void Shutdown();

	// ボタンの更新
	bool Update(UIButtonHandle _handle, bool _isTarget);

	// 指定入力からボタンハンドルを作成
	UIButtonHandle Create(UIButtonInputSource _inputSource);

	// 指定ハンドルの削除
	bool Destroy(UIButtonHandle _handle);

private:
	// ハンドルの分解(ボタンにはリソースという概念がないためSystem側でlookupするがユーザーに漏れないようにprivate)
	UIButton* Lookup(UIButtonHandle _handle);

	// ボタン一つ分のスロット
	struct UIButtonSlot
	{
		UIButton data{}; // 実データ
		std::uint32_t generation{ 0 }; // 世代
	};

private:
	std::vector<UIButtonSlot> slots{}; // ボタンの台帳
	std::stack<int> freeList{}; // ボタンのフリーリスト
};
