#include <cmath>
#include "InputName.h"
#include "../Debug/DebugLogs.h"
#include "InputConstant.h"
#include "MouseInput.h"

// 公開ヘッダーに直接書いたキーコードがWin32のVirtual-Key Codeと一致しているかをコンパイル時に確認する
static_assert(static_cast<int>(MouseCode::Click::LEFT) == VK_LBUTTON, "MouseCode::Click::LEFTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(MouseCode::Click::RIGHT) == VK_RBUTTON, "MouseCode::Click::RIGHTがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(MouseCode::Click::MIDDLE) == VK_MBUTTON, "MouseCode::Click::MIDDLEがVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(MouseCode::Click::SIDE01) == VK_XBUTTON1, "MouseCode::Click::SIDE01がVirtualKeyCodeと不一致\n");
static_assert(static_cast<int>(MouseCode::Click::SIDE02) == VK_XBUTTON2, "MouseCode::Click::SIDE02がVirtualKeyCodeと不一致\n");

void MouseInput::Setup(HWND _hwnd)
{
	DEBUG_ASSERT(_hwnd != nullptr && "MouseInputにnullのHWNDが渡されました\n");
	if (_hwnd == nullptr) return;
	hwnd = _hwnd;
}

void MouseInput::Update()
{
	prevClientCursorPos = currentClientCursorPos; // 保存

	// 取得失敗時に前フレームの移動量を残さない
	cursorDelta = Vector2Int::Zero;

	memcpy(prevClicks, currentClicks, 256);
	BOOL result{ GetKeyboardState(currentClicks) };
	if (!result) memset(currentClicks, 0, 256); // 0リセットで入力を残さない

	// マウスカーソル位置取得(失敗した場合は前回の位置で固定され更新がスキップされる)
	POINT clientPos{}; // コピーにスクリーン座標を入れる 
	result = GetCursorPos(&clientPos);
	if (result)
	{
		// 位置取得に成功した場合のみ変換を行おうとし、失敗したらスキップする
		if (ScreenToClient(hwnd, &clientPos))
		{
			currentClientCursorPos = clientPos; // 変換後の座標を保存

			// この時の実際のマウス移動量を確定して保存
			cursorDelta = { currentClientCursorPos.x - prevClientCursorPos.x, currentClientCursorPos.y - prevClientCursorPos.y };
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

bool MouseInput::IsInputActiveThisFrame()
{
	// マウスでは何か一つ押されているもしくはある程度カーソルを動かしているかホイールを動かしていた場合は操作しているとみなす
	bool isClicked{ false };  // 一つでも押されているか
	bool isCursorMoved{ false }; // マウスが動いたか
	bool isWheelMoved{ false }; // ホイールを動かしたか

	// クリック用の配列
	const int mouseClicks[]{ static_cast<int>(MouseCode::Click::LEFT),  static_cast<int>(MouseCode::Click::RIGHT),  static_cast<int>(MouseCode::Click::MIDDLE),
								static_cast<int>(MouseCode::Click::SIDE01),  static_cast<int>(MouseCode::Click::SIDE02) };
	
	// 今のフレームで一つでも押されているか
	for (int click : mouseClicks)
	{
		// 押し続けているか判定すると入力方法の切り替えが曖昧になるので瞬間で判定する
		if (IsPushed(static_cast<int>(click)))
		{
			isClicked = true;
			break; // 一つでも押していたら抜ける
		}
	}

	// カーソルチェック
	Vector2Int currentDelta{ GetCursorDelta() }; // キャッシュでコールを1回にする
	// 指定量以上マウスカーソルが移動していたら操作しているとみなす
	if (currentDelta.LengthSquared() > MOUSE_MOVE_THRESHOLD_PER_FRAME * MOUSE_MOVE_THRESHOLD_PER_FRAME) isCursorMoved = true;

	// ホイールチェック
	int currentWheelNotchValue{ GetWheelNotchValue() };
	if (std::abs(currentWheelNotchValue) >= 1) isWheelMoved = true;
	
	return isClicked || isCursorMoved || isWheelMoved;
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
	return cursorDelta;
}

bool MouseInput::ResetCursorTracking()
{
	if (!hwnd) return false;

	POINT clientPosition{};
	if (!GetCursorPos(&clientPosition))
	{
		DEBUG_LOG_ERROR("追跡リセット用カーソル位置を取得できませんでした\n");
		return false;
	}
	if (!ScreenToClient(hwnd, &clientPosition))
	{
		DEBUG_LOG_ERROR("追跡リセット用座標を変換できませんでした\n");
		return false;
	}

	// ライブラリ側でカーソルを動かした分を次フレームに持ち越さない
	currentClientCursorPos = clientPosition;
	prevClientCursorPos = clientPosition;
	
	return true;
}

void MouseInput::AddWheelDelta(short _delta)
{
	accumWheel += _delta; // 積算器にためる
}
