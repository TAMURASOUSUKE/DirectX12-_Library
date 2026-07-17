#include "../Src/Facade/TSLib.h"


// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"Flogger", 1280, 720)) return -1;


	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理
;

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}