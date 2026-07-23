#include "../Debug/DebugLogs.h"
#include "InputConstant.h"
#include "MouseInput.h"

void MouseInput::Initialize(HWND _hwnd)
{
	DEBUG_ASSERT(_hwnd != nullptr && "MouseInputにnullのHWNDが渡されました\n");
	if (_hwnd == nullptr) return;
	hwnd = _hwnd;
}

void MouseInput::Update()
{
	prevClientCursorPos = currentClientCursorPos; // 保存
	memcpy(prevClicks, currentClicks, 256);
	BOOL result{ GetKeyboardState(currentClicks) };
	if (!result) memset(currentClicks, 0, 256); // 0リセットで入力を残さない

	// マウスカーソル位置取得(失敗した場合は前回の位置で固定され更新がスキップされる)
	POINT clientPos{}; // コピーにスクリーン座標を入れる 
	result = GetCursorPos(&clientPos);
	if (result)
	{
		// 位置取得に成功した場合のみ変換を行おうとする
		// 失敗したらスキップする
		BOOL clientResult{ false };
		clientResult = ScreenToClient(hwnd, &clientPos);
		if (clientResult)
		{
			currentClientCursorPos = clientPos; // 変換後の座標を保存
		}
		else
		{
			// 失敗したときのみログを出す(Updateのためホットパスではあるが、失敗が稀であるうえ座標が更新されない結果となるため見つけづらいのでログを出す)
			DEBUG_LOG_WARNING("マウスカーソル位置の変換に失敗しました\n");
		}
	}
	else
	{
		// 上記と同様の理由
		DEBUG_LOG_WARNING("マウスカーソル位置の取得に失敗しました\n");
	}

	currentWheel = static_cast<int>(accumWheel); // 加算された値を保存
	accumWheel = 0; // 次のフレームようにリセット

}

bool MouseInput::IsPress(int _click)
{
	return(currentClicks[_click] & MOST_SIGNIFICANT_BIT);
}

bool MouseInput::IsPushed(int _click)
{
	return (currentClicks[_click] & MOST_SIGNIFICANT_BIT) && !(prevClicks[_click] & MOST_SIGNIFICANT_BIT);
}

bool MouseInput::IsReleased(int _click)
{
	return!(currentClicks[_click] & MOST_SIGNIFICANT_BIT) && (prevClicks[_click] & MOST_SIGNIFICANT_BIT);
}

int MouseInput::GetWheelValue()
{
	return currentWheel;
}

int MouseInput::GetWheelNotchValue()
{
	return currentWheel / WHEEL_DELTA_VALUE; // 基準値の120で割って回転数を求める
}

Vector2Int MouseInput::GetCursorPoint()
{
	return Vector2Int{currentClientCursorPos.x, currentClientCursorPos.y};
}

Vector2Int MouseInput::GetCursorDelta()
{
	return Vector2Int{ currentClientCursorPos.x - prevClientCursorPos.x, currentClientCursorPos.y - prevClientCursorPos.y };
}

void MouseInput::AddWheelDelta(short _delta)
{
	accumWheel += _delta; // 積算器にためる
}