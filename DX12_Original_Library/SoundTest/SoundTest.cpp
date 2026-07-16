#include "../Src/Facade/TSLib.h"


/*
	パッケージ化前なので、DirectXTexの参照も入れていますが使っていません。
*/
// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"SoundTest", 1280, 720)) return -1;

	// 各ハンドルのロード
	SoundHandle testSound{ Sound::LoadSound("Res/Test.wav") };
	SoundHandle testSound02{ Sound::LoadSound("Res/Phuniaya_2.wav") };
	SoundHandle testSound03{ Sound::LoadSound("Res/Better_Days.wav") };
	float testVolume{ 0.8f }; // テスト用音量

	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理

		// 音のテスト
		if (Input::IsKeyPushed(KeyCode::Button::G)) Sound::PlaySE(testSound);
		if (Input::IsKeyPushed(KeyCode::Button::D2)) Sound::PlayBGM(testSound02, false, testVolume);
		if (Input::IsKeyPushed(KeyCode::Button::D3)) Sound::PlayBGM(testSound03, true, 0.8f);
		if (Input::IsKeyPushed(KeyCode::Button::S)) Sound::StopBGM();
		if (Input::IsKeyPushed(KeyCode::Button::RETURN)) Sound::EndBGM();
		if (Input::IsKeyPress(KeyCode::Button::RIGHT)) Sound::SetVolume(testSound02, testVolume);
		if (Input::IsKeyPress(KeyCode::Button::LEFT)) Sound::SetVolume(testSound02, testVolume);
		if (Input::IsKeyPushed(KeyCode::Button::F)) Sound::CrossfadeBGM(testSound02, false, 10.0f);

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
