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

bool UIButtonSystem::Update(UIButtonHandle _handle, bool _isTarget)
{

	return false;
}

UIButtonHandle UIButtonSystem::Create(UIButtonInputSource _inputSource)
{
	if (!_inputSource.IsValid())
	{
		DEBUG_LOG_ERROR("不正な入力元が渡されました\n");
		return UIButtonHandle{};
	}
	// 入力をもとにボタンを作成する
	UIButton button{ _inputSource };

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
	slots[index].generation++; // 世代を上げて破棄前のハンドルを再利用できなくする
	freeList.push(index); // この位置を使えるようにする
	return true;
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
	return &slot.data;
}
