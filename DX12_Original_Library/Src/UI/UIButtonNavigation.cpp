#include <cstddef>
#include <algorithm>
#include "../Debug/DebugLogs.h"
#include "UIConstant.h"
#include "UIButtonNavigation.h"

bool UIButtonNavigation::Setup(int _columnCount)
{
	if (_columnCount <= 0)
	{
		DEBUG_LOG_ERROR("UIButtonNavigationの列数には1以上を指定してください ColumnCount : {}\n", _columnCount);
		return false;
	}
	Clear(); // 再度初期化されたときにリセットをかける
	columnCount = _columnCount;
	return true;
}

void UIButtonNavigation::Shutdown()
{
	// ボタンを解除して列もリセット
	Clear();
	columnCount = 0;
}

bool UIButtonNavigation::AddButton(UIButtonHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("UIButtonNavigationへ無効なハンドルが渡されました\n");
		return false;
	}
	if (columnCount <= 0)
	{
		DEBUG_LOG_ERROR("UIButtonNavigationがSetupされていません\n");
		return false;
	}

	// ボタンの重複登録防止
	const auto found{ std::find(buttons.begin(), buttons.end(), _handle) };
	if (found != buttons.end())
	{
		DEBUG_LOG_WARNING("UIButtonNavigationへ同じボタンが重複登録されました\n");
		return false;
	}
	if (buttons.size() >= MAX_UI_BUTTON_COUNT)
	{
		DEBUG_LOG_ERROR("ボタンの登録上限に達しています\n");
		return false;
	}

	buttons.push_back(_handle);

	// 最初のボタンを追加したときには選択状態にする
	if (selectedIndex < 0) selectedIndex = 0;
	return true;
}

bool UIButtonNavigation::RemoveButton(UIButtonHandle _handle)
{
	// 指定したハンドルがあるか探索する
	const auto found{ std::find(buttons.begin(), buttons.end(), _handle) };
	if (found == buttons.end())
	{
		DEBUG_LOG_WARNING("UIButtonNavigationに登録されていないボタンを削除しようとしました\n");
		return false;
	}

	// 削除するボタンのインデックスを特定
	const int  removedIndex{ static_cast<int>(std::distance(buttons.begin(), found)) };
	buttons.erase(found);

	// 全ボタンがなくなった時は選択インデックスを-1にする
	if (buttons.empty())
	{
		selectedIndex = -1;
		return true; // 正常な削除の結果空になるので正常終了
	}

	// 選択位置より前が消えた場合はeraseによってずれた分を補正
	if (selectedIndex > removedIndex)
	{
		selectedIndex--;
	}
	// 選択中のボタンが削除された場合
	else if (selectedIndex == removedIndex)
	{
		const int lastIndex{ static_cast<int>(buttons.size() - 1) }; // 最後尾
		selectedIndex = std::min(removedIndex, lastIndex);
	}
	return true;
}

void UIButtonNavigation::Clear()
{
	buttons.clear();
	selectedIndex = -1;
}

void UIButtonNavigation::ResetSelection()
{
	selectedIndex = buttons.empty() ? -1 : 0;
}

bool UIButtonNavigation::Move(UINavigationDirection _direction)
{
	// 移動方向がないならfalse
	if (_direction == UINavigationDirection::None) return false;
	// ボタンが空や列が0以下ならfalse
	if (buttons.empty() || columnCount <= 0) return false;

	// 内部状態が壊れているなら最初のボタンへ
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(buttons.size()))
	{
		selectedIndex = 0;
		return true;
	}

	const int prevIndex{ selectedIndex };
	const int buttonCount{ static_cast<int>(buttons.size()) };

	// 現在いる行の先頭と末尾を求める
	const int rowStart{ (selectedIndex / columnCount) * columnCount };
	const int rowEnd{ std::min(rowStart + columnCount, buttonCount) };

	const int currentColumn{ selectedIndex % columnCount };

	switch (_direction)
	{
	case UINavigationDirection::Up:

		if (selectedIndex - columnCount >= 0)
		{
			// 一つ上の段へ
			selectedIndex -= columnCount;
		}
		else
		{
			// 1番上なら同じ列の一番下へ
			int lastIndexColumn{ currentColumn };
			while (lastIndexColumn + columnCount < buttonCount)
			{
				lastIndexColumn += columnCount; // 一番下まで足し続ける
			}
			selectedIndex = lastIndexColumn;
		}

		break;
	case UINavigationDirection::Down:

		// 下側がボタン配列内であれば
		if (selectedIndex + columnCount < buttonCount) selectedIndex += columnCount;
		// 1番下なら同列の一番上に移動する
		else selectedIndex = currentColumn;

		break;
	case UINavigationDirection::Left:
		// 行の左橋なら同じ行の右端へ
		if (selectedIndex == rowStart) selectedIndex = rowEnd - 1;
		else	selectedIndex--;

		break;
	case UINavigationDirection::Right:
		// 行の右端なら同じ行の左端へ
		if (selectedIndex + 1 >= rowEnd) selectedIndex = rowStart;
		else selectedIndex++;
		break;
	case UINavigationDirection::None:
	default:
		break;
	}
	return selectedIndex != prevIndex; // 移動できたかを前の位置と比較して返す
}

bool UIButtonNavigation::SetSelected(UIButtonHandle _handle)
{
	const auto found{ std::find(buttons.begin(), buttons.end(), _handle) }; // 指定したハンドルを探す
	if (found == buttons.end())
	{
		DEBUG_LOG_WARNING("選択しようとしたボタンがUINavigationに登録されていません\n");
		return false;
	}
	selectedIndex = static_cast<int>(std::distance(buttons.begin(), found)); // 指定したハンドルが存在する場所までのインデックスにする
	return true;
}

UIButtonHandle UIButtonNavigation::GetSelected() const
{
	// 範囲チェック
	if (selectedIndex < 0 || selectedIndex >= static_cast<int>(buttons.size())) return UIButtonHandle{};
	return buttons[static_cast<std::size_t>(selectedIndex)];
}

bool UIButtonNavigation::IsSelected(UIButtonHandle _handle) const
{
	return _handle.IsValid() && _handle == GetSelected();
}

bool UIButtonNavigation::Contains(UIButtonHandle _handle) const
{
	// 指定ハンドルが配列内に入っているか
	return std::find(buttons.begin(), buttons.end(), _handle) != buttons.end();
}
