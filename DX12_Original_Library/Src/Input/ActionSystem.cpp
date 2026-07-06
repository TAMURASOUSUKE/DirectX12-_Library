#include <variant>
#include "../Debug/DebugLogs.h"
#include "GamePadInput.h"
#include "KeyboardInput.h"
#include "MouseInput.h"
#include "ActionSystem.h"

namespace
{
	struct BindingVisitor
	{
		// 物理状態を持ってくる
		KeyboardInput& kb;
		MouseInput& ms;
		GamePadInput& pad;

		bool operator()(KeyCode::Button _k) const { return kb.IsPress(static_cast<int>(_k)); }
		bool operator()(PadCode::Button _p) const { return pad.IsPress(static_cast<int>(_p)); }
		bool operator()(PadCode::Trigger _t) const { return pad.IsPress(_t); }
		bool operator()(MouseCode::Click _c) const { return ms.IsPress(static_cast<int>(_c)); }
	};
}

// ユーザーが定義したアクション分のvectorを確保する
void ActionSystem::Setup(int _actionCount)
{
	DEBUG_ASSERT((_actionCount > 0) && "抽象化入力初期化に0値が渡されています\n");
	if (_actionCount <= 0)
	{
		// 0以下ならクリアする
		actions.clear();
		return;
	}
	actions.resize(_actionCount);
}
// 該当アクション,設定したいキーで抽象化を行う
void ActionSystem::SetAction(int _action, Binding _binding)
{
	// サイズチェック
	if (!IsInSizeLimit(_action)) return;
	actions[_action].bindings.push_back(_binding);
}
// 各状態を更新
void ActionSystem::Update(KeyboardInput& _kb, MouseInput& _ms, GamePadInput& _pad)
{
	BindingVisitor visitor{_kb, _ms, _pad};
	for (auto& action : actions)
	{
		action.prev = action.current; // 保存
		bool isPress{ false };
		for (const Binding& b : action.bindings)
		{
 			isPress |= std::visit(visitor, b); // 各コードで押されているかを探索する
			if (isPress) break; // 押されていれば即終わる
		}
		action.current = isPress;
	}
}

bool ActionSystem::IsPress(int _action)
{
	// サイズチェック
	if (!IsInSizeLimit(_action)) return false;
	return actions[_action].current;
}
bool ActionSystem::IsPushed(int _action)
{
	// サイズチェック
	if (!IsInSizeLimit(_action)) return false;
	return (actions[_action].current) && (!actions[_action].prev);
}
bool ActionSystem::IsReleased(int _action)
{
	// サイズチェック
	if (!IsInSizeLimit(_action)) return false;
	return (!actions[_action].current) && (actions[_action].prev);
}

bool ActionSystem::IsInSizeLimit(int _value) const
{
	DEBUG_ASSERT((_value >= 0 && _value < static_cast<int>(actions.size())) && "サイズをオーバーしました");
	if (_value < 0 || _value >= static_cast<int>(actions.size()))
	{
		return false;
	}
	return true;
}