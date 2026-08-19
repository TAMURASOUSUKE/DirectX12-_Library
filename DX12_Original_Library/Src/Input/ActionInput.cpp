#include <variant>
#include <utility>
#include "../Debug/DebugLogs.h"
#include "GamePadInput.h"
#include "KeyboardInput.h"
#include "MouseInput.h"
#include "ActionInput.h"

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
void ActionInput::SetupActionCount(int _actionCount)
{
	DEBUG_ASSERT((_actionCount > 0) && "抽象化入力初期化に0値が渡されています\n");
	if (_actionCount <= 0)
	{
		actions.clear();
		return;
	}
	actions.clear(); // 古い物を消してから確保する
	actions.resize(static_cast<std::size_t>(_actionCount));
}
// 該当アクション,設定したいキーで抽象化を行う
bool ActionInput::AddActionBinding(int _action, Binding _binding)
{
	// サイズチェック
	std::size_t index{ static_cast<std::size_t>(_action) };
	if (!IsInSizeLimit(index)) return false;
	actions[index].bindings.push_back(std::move(_binding));
	return true;
}

// 各状態を更新
void ActionInput::Update(KeyboardInput& _kb, MouseInput& _ms, GamePadInput& _pad)
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

bool ActionInput::IsPress(int _action)
{
	// サイズチェック
	std::size_t index{ static_cast<std::size_t>(_action) };
	if (!IsInSizeLimit(index)) return false;
	return actions[index].current;
}

bool ActionInput::IsPushed(int _action)
{
	// サイズチェック
	std::size_t index{ static_cast<std::size_t>(_action) };
	if (!IsInSizeLimit(index)) return false;
	return (actions[index].current) && (!actions[index].prev);
}

bool ActionInput::IsReleased(int _action)
{
	// サイズチェック
	std::size_t index{ static_cast<std::size_t>(_action) };
	if (!IsInSizeLimit(index)) return false;
	return (!actions[index].current) && (actions[index].prev);
}

bool ActionInput::IsInSizeLimit(std::size_t _value) const
{
	DEBUG_ASSERT(_value < static_cast<int>(actions.size()) && "サイズをオーバーしました");
	if (_value >= actions.size())
	{
		return false;
	}
	return true;
}
