#include <utility>
#include "Input.h" // UIはInputとGfxを組み合わせて成り立つものなのでこの依存は許容
#include "../UI/UIButtonInputSource.h"
#include "../UI/UIButtonSystem.h"
#include "UIInternal.h"
#include "UI.h"

namespace
{
	UIButtonSystem buttonSystem{};
}

namespace 
{
	// 指定されたキー入力からキーの状態を調べてboolにまとめるクラスに渡して返す
	UIButtonInputSource MakeButtonInputSource(KeyCode::Button _key)
	{
		auto pushed = [_key]() { return Input::IsKeyPushed(_key); }; // 押した瞬間
		auto held = [_key]() { return Input::IsKeyPress(_key); }; // 押している間
		auto released = [_key]() { return Input::IsKeyReleased(_key); }; // 離した瞬間
		return UIButtonInputSource(pushed, held, released);
	}
	// 指定されたパッド入力から状態を調べてboolにまとめるクラスに渡して返す
	UIButtonInputSource MakeButtonInputSource(PadCode::Button _key)
	{
		//auto pushed = [_key]() { return Input::IsKeyPushed(_key); }; // 押した瞬間
		//auto held = [_key]() { return Input::IsKeyPress(_key); }; // 押している間
		//auto released = [_key]() { return Input::IsKeyReleased(_key); }; // 離した瞬間
		//return UIButtonInputSource(pushed, held, released);
	}
}

void UIInternal::Initialize()
{
	buttonSystem.Setup();
}

void UIInternal::Finish()
{
	buttonSystem.Shutdown();
}

void UIInternal::BeginFrame()
{
	buttonSystem.UpdateAll();
}

UIButtonHandle UI::Create(KeyCode::Button _key, ButtonTargetQuery _targetQuery)
{
	const UIButtonInputSource inputSource{ MakeButtonInputSource(_key) }; // 指定キーをそれぞれの状態に適用してboolにまとめる
	return buttonSystem.Create(inputSource, std::move(_targetQuery));
}

bool UI::SetOnActivated(UIButtonHandle _handle, ButtonEventCallback _callback)
{
	return buttonSystem.SetOnActivated(_handle, _callback);
}

bool UI::DestroyButton(UIButtonHandle _handle)
{
	return buttonSystem.Destroy(_handle);
}
