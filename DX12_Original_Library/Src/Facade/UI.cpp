#include "Input.h" // UIはInputとGfxを組み合わせて成り立つものなのでこの依存は許容
#include "../UI/UIButtonInputSource.h"
#include "UI.h"

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
}
