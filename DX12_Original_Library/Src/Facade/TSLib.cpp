#include "GfxInternal.h"
#include "InputInternal.h"
#include "TSLib.h"

// 初期化
bool TSLib::Initialize(const wchar_t* _title, int _width, int _height)
{
	return GfxInternal::Initialize(_title, _width, _height); // グラフィックの初期化とウィンドウ作成
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