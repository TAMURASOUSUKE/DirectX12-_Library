#include <utility>
#include "../Core/Handle/HandlePacking.h"
#include "../Debug/DebugLogs.h"
#include "UIConstant.h"
#include "UIButtonSystem.h"

void UIButtonSystem::Setup()
{
	Shutdown(); // 再初期化対策
	slots.reserve(MAX_UI_BUTTON_COUNT); // 最大容量をあらかじめ確保
}

void UIButtonSystem::Shutdown()
{
	slots.clear(); // 配列クリア
	while (!freeList.empty())
	{
		// 中身が空になるまでpop
		freeList.pop();
	}
}

void UIButtonSystem::UpdateAll(UIButtonHandle _navigationTarget, bool _useNavigationTarget)
{
	for (std::size_t i = 0; i < slots.size(); i++)
	{
		UIButtonSlot& slot{ slots[i] };
		if (!slot.isAlive) continue; // ボタンが生きてい無ければスキップ

		bool isTarget{ false };

		if (_useNavigationTarget)
		{
			// 現在のスロットから有効なUIButtonHandleを再構築する
			const int packed{ Pack(static_cast<int>(i), static_cast<int>(slot.generation)) };
			const UIButtonHandle currentHandle{ PassKey{}, packed };
			// ナビゲーションが選択しているボタンだけを対象にする
			isTarget = currentHandle == _navigationTarget;
		}
		else
		{
			// マウスなどの外部判定を使用する
			isTarget = slot.targetQuery ? slot.targetQuery() : false;
 		}
		slot.data.Update(isTarget); // スロットが操作対象かをisTargetで判断して更新する
	}
}

bool UIButtonSystem::SetOnActivated(UIButtonHandle _handle, UIButton::EventCallback _callback)
{
	UIButton* data{ Lookup(_handle) };
	if (!data)
	{
		DEBUG_LOG_ERROR("無効なハンドルが渡されました\n");
		return false;
	}
	data->SetOnActivated(std::move(_callback));
	return true;
}

UIButtonHandle UIButtonSystem::Create(UIButtonInputSource _inputSource, TargetQuery _targetQuery)
{
	if (!_inputSource.IsValid())
	{
		DEBUG_LOG_ERROR("不正な入力元が渡されました\n");
		return UIButtonHandle{};
	}

	if (!_targetQuery)
	{
		DEBUG_LOG_ERROR("ボタンの対象判定関数が登録されていません\n");
		return UIButtonHandle{};
	}

	// 新しく確保できるかもしくは再利用できるか
	const bool canRegister{ !freeList.empty() || slots.size() < MAX_UI_BUTTON_COUNT };
	DEBUG_ASSERT(canRegister && "UIボタンの登録上限に達しました\n"); // 致命的なエラーなのでデバッグ時に止める
	if (!canRegister) return UIButtonHandle{};

	// 入力をもとにボタンを作成する
	UIButton data{ std::move(_inputSource) };
	int index{ 0 };
	if (!freeList.empty())
	{
		index = freeList.top(); // Destroyされたところから持ってくる
		freeList.pop();
		slots[index].data = std::move(data);
		slots[index].targetQuery = std::move(_targetQuery);
		slots[index].isAlive = true;
	}
	else
	{
		index = static_cast<int>(slots.size()); // 空きがないなら新しく作る
		slots.push_back({ std::move(data), 0 , std::move(_targetQuery), true}); // 新しく確保したので世代は0
	}
	const int pack{ Pack(index, static_cast<int>(slots[index].generation))};
	return UIButtonHandle{ PassKey{}, pack };
}

bool UIButtonSystem::Destroy(UIButtonHandle _handle)
{
	// ハンドルの正当性をLookupで検査する
	UIButton* data{ Lookup(_handle) };
	if (!data)
	{
		DEBUG_LOG_ERROR("不正なハンドルが渡されました\n");
		return false;
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };
	slots[index].data = UIButton{};
	slots[index].targetQuery = {};
	slots[index].generation++; // 世代を上げて破棄前のハンドルを再利用できなくする
	slots[index].isAlive = false;
	freeList.push(index); // この位置を使えるようにする
	return true;
}

UIButtonVisualState UIButtonSystem::GetVisualState(UIButtonHandle _handle)
{
	UIButton* data{ Lookup(_handle) };
	if (!data) return UIButtonVisualState::Normal; // Lookup失敗は内部で警告を出して通常表示
	return data->GetVisualState();
}

UIButton* UIButtonSystem::Lookup(UIButtonHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}
	int packed{ _handle.GetRaw(PassKey{}) }; // 内部ハンドルを取り出す
	int index{ UnpackIndex(packed) }; // index取り出し
	if (index < 0 || index >= static_cast<int>(slots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	UIButtonSlot& slot{ slots[index] };
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	if (!slot.isAlive)
	{
		DEBUG_LOG_WARNING("有効でないボタンです\n");
		return nullptr; // 生存チェック
	}
	return &slot.data;
}
