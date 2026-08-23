#include "Input.h" // inputはボタンを組み立てる上で必要な下位部品なので依存する
#include "../Debug/DebugLogs.h"
#include "../UI/UIButtonInputSource.h"
#include "../UI/UIButtonSystem.h"
#include "../UI/UIButtonNavigation.h"
#include "../UI/UINavigationRepeater.h"
#include "UIInternal.h"
#include "UI.h"

namespace
{
	std::function<Vector2()> navigationAxisQuery{}; // 方向をVector2で返すAxisのナビゲーション関数

	UIButtonSystem buttonSystem{}; // UIボタンを操作するためのシステム
	UIButtonNavigation buttonNavigation{}; // UIボタンをナビゲーションする
	UINavigationRepeater navigationRepeater{}; // 繰り返し選択する場合等に使うシステム

	UIInternal::FrameContext frameContext{}; // 1フレームで使う情報

	bool isNavigationReady{ false };
	bool useNavigationTarget{ false };
}

namespace 
{
	// 指定されたキー入力からキーの状態を調べてboolにまとめるクラスに渡して返す
	UIButtonInputSource MakeButtonInputSource(KeyCode::Button _key)
	{
		auto pushed = [_key]() { return Input::IsKeyPushed(_key); }; // 押した瞬間
		auto held = [_key]() { return Input::IsKeyPress(_key); }; // 押している間
		auto released = [_key]() { return Input::IsKeyReleased(_key); }; // 離した瞬間
		return UIButtonInputSource(std::move(pushed), std::move(held), std::move(released));
	}
	// 指定されたパッド入力から状態を調べてboolにまとめるクラスに渡して返す
	UIButtonInputSource MakeButtonInputSource(PadCode::Button _button)
	{
		auto pushed = [_button]() { return Input::IsPadPushed(_button); }; // 押した瞬間
		auto held = [_button]() { return Input::IsPadPress(_button); }; // 押している間
		auto released = [_button]() { return Input::IsPadReleased(_button); }; // 離した瞬間
		return UIButtonInputSource(std::move(pushed), std::move(held), std::move(released));
	}
	// 指定されたマウス入力から状態を調べてboolにまとめるクラスに渡して返す
	UIButtonInputSource MakeButtonInputSource(MouseCode::Click _click)
	{
		auto pushed = [_click]() { return Input::IsMousePushed(_click); }; // 押した瞬間
		auto held = [_click]() { return Input::IsMousePress(_click); }; // 押している間
		auto released = [_click]() { return Input::IsMouseReleased(_click); }; // 離した瞬間
		return UIButtonInputSource(std::move(pushed), std::move(held), std::move(released));
	}
	// 指定された抽象化入力から状態を調べてboolにまとめるクラスに渡して返す
	UIButtonInputSource MakeButtonInputSource(int _actionIndex)
	{
		auto pushed = [_actionIndex]() { return Input::Detail::IsActionPushedImpl(_actionIndex); };
		auto held = [_actionIndex]() { return Input::Detail::IsActionPressImpl(_actionIndex); };
		auto released = [_actionIndex]() { return Input::Detail::IsActionReleasedImpl(_actionIndex); };
		return UIButtonInputSource(std::move(pushed), std::move(held), std::move(released));
	}
	// 標準的な入力をまとめた関数
	UIButtonInputSource MakeDefaultButtonInputSource()
	{
		// キー : Enter パッド : A クリック : 左
		auto pushed = []() { return Input::IsKeyPushed(KeyCode::Button::RETURN) || Input::IsPadPushed(PadCode::Button::A) || Input::IsMousePushed(MouseCode::Click::LEFT); }; // 押した瞬間
		auto held = []() { return Input::IsKeyPress(KeyCode::Button::RETURN) || Input::IsPadPress(PadCode::Button::A) || Input::IsMousePress(MouseCode::Click::LEFT); }; // 押している間
		auto released = []() { return Input::IsKeyReleased(KeyCode::Button::RETURN) || Input::IsPadReleased(PadCode::Button::A) || Input::IsMouseReleased(MouseCode::Click::LEFT); }; // 離した瞬間
		return UIButtonInputSource(std::move(pushed), std::move(held), std::move(released));
	}

	Vector2 MakeNavigationInput()
	{
		Vector2 digitalInput{ Vector2::Zero };

		if (Input::IsKeyPress(KeyCode::Button::LEFT) || Input::IsPadPress(PadCode::Button::LEFT)) digitalInput.x -= 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::RIGHT) || Input::IsPadPress(PadCode::Button::RIGHT)) digitalInput.x += 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::UP) || Input::IsPadPress(PadCode::Button::UP)) digitalInput.y -= 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::DOWN) || Input::IsPadPress(PadCode::Button::DOWN)) digitalInput.y += 1.0f;

		// digital入力があればそちらを優先する
		if (digitalInput.LengthSquared() > 0.0f) return digitalInput;

		// ユーザーが登録した抽象化Axis
		if (navigationAxisQuery)
		{
			const Vector2 abstractInput{ navigationAxisQuery() };
			if (abstractInput.LengthSquared() > 0.0f) return abstractInput;
		}

		// 入力がなければ左スティックを使用する
		return Input::GetPadStickValue(PadCode::Stick::LEFT);
	}

	bool IsMouseActivityDetected()
	{
		const Vector2Int delta{ Input::GetMouseDelta() };

		return delta.x != 0 || delta.y != 0 || Input::IsMousePushed(MouseCode::Click::LEFT);
	}

	// 仮想マウス座標を取得する
	bool TryGetVirtualMousePoint(Vector2& _outPoint)
	{
		const Vector2Int mousePoint{ Input::GetMousePoint() };
		const Vector2Int clientSize{ frameContext.clientSize };
		const Vector2Int virtualSize{ frameContext.virtualSize };

		// 値チェック
		if (clientSize.x <= 0 || clientSize.y <= 0 || virtualSize.x <= 0 || virtualSize.y <= 0) return false;

		// 実画面から仮想座標へ変換する必要がある
		_outPoint.x = static_cast<float>(mousePoint.x) * static_cast<float>(virtualSize.x) / static_cast<float>(clientSize.x);
		_outPoint.y = static_cast<float>(mousePoint.y) * static_cast<float>(virtualSize.y) / static_cast<float>(clientSize.y);
		return true;
	}

	// 矩形のTargetQuery
	UI::ButtonTargetQuery MakeRectTargetQuery(Rect _rect)
	{
		return [_rect]()
			{
				Vector2 mousePoint{ Vector2::Zero };
				if (!TryGetVirtualMousePoint(mousePoint)) return false;

				const Vector2 min{ _rect.GetMinPos() };
				const Vector2 max{ _rect.GetMaxPos() };

				// マウス座標と矩形のあたり判定をおこなう
				return mousePoint.x >= min.x && mousePoint.x <= max.x && mousePoint.y >= min.y && mousePoint.y <= max.y;
			};
	}
}

void UIInternal::Initialize()
{
	buttonSystem.Setup();

	buttonNavigation.Shutdown();
	navigationRepeater.Reset();

	navigationAxisQuery = {};
	isNavigationReady = false;
	useNavigationTarget = false;
}

void UIInternal::Finish()
{
	buttonNavigation.Shutdown();
	navigationRepeater.Reset();
	buttonSystem.Shutdown();

	navigationAxisQuery = {};
	isNavigationReady = false;
	useNavigationTarget = false;
}

void UIInternal::BeginFrame(const FrameContext& _context)
{
	frameContext = _context;

	if (!isNavigationReady)
	{
		// ナビゲーション未使用なら従来のTargetQueryだけで更新
		buttonSystem.UpdateAll(UIButtonHandle{}, false);
		return;
	}

	// マウスを動かしたら従来のTargetQueryへ
	if (IsMouseActivityDetected())
	{
		useNavigationTarget = false;
		navigationRepeater.Reset();
	}

	// 時間と入力から方向を出す
	const UINavigationDirection direction{ navigationRepeater.Update(MakeNavigationInput(), frameContext.unscaledDeltaTime) };

	if (direction != UINavigationDirection::None)
	{
		buttonNavigation.Move(direction);
		useNavigationTarget = true;
	}

	// 移動後のハンドルを同じフレームでボタンへ渡す
	buttonSystem.UpdateAll(buttonNavigation.GetSelected(), useNavigationTarget);
}

UIButtonHandle UI::Detail::CreateActionImpl(int _actionIndex, ButtonTargetQuery _targetQuery)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_actionIndex) }; // 指定アクションをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, std::move(_targetQuery));
}

UIButtonHandle UI::Create(KeyCode::Button _key, ButtonTargetQuery _targetQuery)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_key) }; // 指定キーをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, std::move(_targetQuery));
}

UIButtonHandle UI::Create(PadCode::Button _button, ButtonTargetQuery _targetQuery)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_button) }; // 指定ボタンをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, std::move(_targetQuery));
}

UIButtonHandle UI::Create(MouseCode::Click _click, ButtonTargetQuery _targetQuery)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_click) }; // 指定クリックをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, std::move(_targetQuery));
}

UIButtonHandle UI::Detail::CreateActionRectImpl(int _actionIndex, Rect _rect)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_actionIndex) }; // 指定アクションをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, MakeRectTargetQuery(_rect));
}

UIButtonHandle UI::Create(KeyCode::Button _key, Rect _hitRect)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_key) }; // 指定キーをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, MakeRectTargetQuery(_hitRect));
}

UIButtonHandle UI::Create(PadCode::Button _button, Rect _hitRect)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_button) }; // 指定ボタンをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, MakeRectTargetQuery(_hitRect));
}

UIButtonHandle UI::Create(MouseCode::Click _click, Rect _hitRect)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_click) }; // 指定クリックをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, MakeRectTargetQuery(_hitRect));
}

UIButtonHandle UI::Create(Rect _hitRect)
{
	return buttonSystem.Create(MakeDefaultButtonInputSource(), MakeRectTargetQuery(_hitRect));
}

bool UI::SetOnTarget(UIButtonHandle _handle, ButtonEventCallback _callback)
{
	return buttonSystem.SetOnTarget(_handle, _callback);
}

bool UI::SetOnPushed(UIButtonHandle _handle, ButtonEventCallback _callback)
{
	return buttonSystem.SetOnPushed(_handle, _callback);
}

bool UI::SetOnHeld(UIButtonHandle _handle, ButtonEventCallback _callback)
{
	return buttonSystem.SetOnHeld(_handle, _callback);
}

bool UI::SetOnReleased(UIButtonHandle _handle, ButtonEventCallback _callback)
{
	return buttonSystem.SetOnReleased(_handle, _callback);
}

bool UI::SetOnActivated(UIButtonHandle _handle, ButtonEventCallback _callback)
{
	return buttonSystem.SetOnActivated(_handle, _callback);
}

bool UI::SetOnCanceled(UIButtonHandle _handle, ButtonEventCallback _callback)
{
	return buttonSystem.SetOnCanceled(_handle, _callback);
}

bool UI::DestroyButton(UIButtonHandle _handle)
{
	if (!buttonSystem.Destroy(_handle)) return false;
	// 追加されているのを確認してから
	if (buttonNavigation.Contains(_handle)) buttonNavigation.RemoveButton(_handle);

	return true;
}

bool UI::SetupNavigation(int _columnCount, UINavigationRepeatSettings _setting)
{
	if (!buttonNavigation.Setup(_columnCount)) return false;
	if (!navigationRepeater.Setup(_setting)) return false;

	isNavigationReady = true;
	useNavigationTarget = true;
	return true;
}

bool UI::AddNavigationButton(UIButtonHandle _handle)
{
	if (!isNavigationReady)
	{
		DEBUG_LOG_ERROR("SetupNavigationより前にボタンを登録しようとしました\n");
		return false;
	}

	return buttonNavigation.AddButton(_handle);
}

bool UI::Detail:: SetNavigationAxisImpl(int _axisIndex)
{
	if (!isNavigationReady)
	{
		DEBUG_LOG_ERROR("SetupNavigationより前にNavigationAxisを設定しようとしました\n");
		return false;
	}

	navigationAxisQuery = [_axisIndex]() { return Input::Detail::GetAxisValueImpl(_axisIndex); };
	return true;
}

bool UI::RemoveNavigationButton(UIButtonHandle _handle)
{
	if (!isNavigationReady) return false;

	return buttonNavigation.RemoveButton(_handle);
}

void UI::ResetNavigationSelection()
{
	if (!isNavigationReady) return;

	buttonNavigation.ResetSelection();
	useNavigationTarget = true;
}

UIButtonHandle UI::GetSelectedButton()
{
	if (!isNavigationReady) return UIButtonHandle{};

	return buttonNavigation.GetSelected();
}

UIButtonVisualState UI::GetVisualState(UIButtonHandle _handle)
{
	return buttonSystem.GetVisualState(_handle);
}
