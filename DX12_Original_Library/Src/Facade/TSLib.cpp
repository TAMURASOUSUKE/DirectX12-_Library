#include "../Debug/DebugLogs.h"
#include "GfxInternal.h"
#include "InputInternal.h"
#include "TSLib.h"

// 初期化
bool TSLib::Initialize(const wchar_t* _title, int _width, int _height)
{
	bool result{ false };
	result = GfxInternal::Initialize(_title, _width, _height); // グラフィックの初期化とウィンドウ作成
	DEBUG_ASSERT(result && "ゲームの初期化に失敗しました\n");
	if (!result) return result;
	result = InputInternal::Initialize(GfxInternal::GetHWND());
	DEBUG_ASSERT(result && "入力処理の初期化に失敗しました\n");
	if (!result) return result;

	// コールバックの配線接続 : ラムダで渡す
	GfxInternal::SetOnWheel([](short _d) { InputInternal::AddMouseWheelDelta(_d); });
	
	return result;
}

void TSLib::BeginFrame()
{
	InputInternal::BeginFrame(); // 入力の最初の処理
	GfxInternal::BeginFrame(); // グラフィックのフレーム最初の処理
}

void TSLib::EndFrame()
{
	InputInternal::EndFrame();
	GfxInternal::EndFrame(); // グラフィックのフレーム最後の処理
}

void TSLib::Finish()
{
	InputInternal::Finish();
	GfxInternal::Finish(); // 終了処理
}