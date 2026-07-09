#include "../Src/Facade/TSLib.h"

// 音のテスト等を行う
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"SoundTest", 1280, 720)) return -1;

	SoundHandle testSound{ Sound::LoadSound("Test.wav")};

	while (Gfx::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理



		Gfx::ClearScreen(); // 画面クリア(黒)


		TSLib::EndFrame(); // フレーム最後の処理
	}

	TSLib::Finish(); // 終了処理
}