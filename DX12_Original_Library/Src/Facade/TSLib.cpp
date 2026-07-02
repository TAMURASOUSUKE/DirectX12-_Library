#include "../Debug/DebugLogs.h"
#include "GfxInternal.h"
#include "InputInternal.h"
#include "TSLib.h"

// 初期化
bool TSLib::Initialize(const wchar_t* _title, int _width, int _height)
{
	bool result{ false };
	result = InputInternal::Initialize();
	result = GfxInternal::Initialize(_title, _width, _height); // グラフィックの初期化とウィンドウ作成
	if (!result) DEBUG_ASSERT(result && "ゲームの初期化に失敗しました\n");
	return result;
}

void TSLib::BeginFrame()
{
	InputInternal::BeginFrame(); // 入力の最初の処理
	GfxInternal::BeginFrame(); // グラフィックのフレーム最初の処理
}

void TSLib::EndFrame()
{
	GfxInternal::EndFrame(); // グラフィックのフレーム最後の処理
}

void TSLib::Finish()
{
	GfxInternal::Finish(); // 終了処理
}