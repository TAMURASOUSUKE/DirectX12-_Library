#include <string> 
#include <algorithm>
#include "../Src/Facade/TSLib.h"


/*
	パッケージ化前なので、DirectXTexの参照も入れていますが使っていません。
*/
// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"SoundTest", 1280, 720)) return -1;

	Time::SetTargetFPS(60);

	// 各ハンドルのロード
	SoundHandle testSound{ Sound::LoadSound("Res/Test.wav") };
	SoundHandle testSound02{ Sound::LoadSound("Res/Phuniaya_2.wav") };
	SoundHandle testSound03{ Sound::LoadSound("Res/Better_Days.wav") };
	float testVolume{ 0.8f }; // テスト用音量
	float crossFadeTime{ 0.0f };
	bool isSetVolume{ false };

	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理


		// 音のテスト
		if (Input::IsKeyPushed(KeyCode::Button::G)) Sound::PlaySE(testSound);
		if (Input::IsKeyPushed(KeyCode::Button::D1)) Sound::PlayBGM(testSound02, false, testVolume);
		if (Input::IsKeyPushed(KeyCode::Button::D2)) Sound::PlayBGM(testSound03, true, 0.8f);
		if (Input::IsKeyPushed(KeyCode::Button::S)) Sound::StopBGM();
		if (Input::IsKeyPushed(KeyCode::Button::RETURN)) Sound::EndBGM();
		if (Input::IsKeyPress(KeyCode::Button::RIGHT))
		{
			testVolume += 0.01f;
			isSetVolume = true;
		}
		else if (Input::IsKeyPress(KeyCode::Button::LEFT))
		{
			testVolume -= 0.01f;
			isSetVolume = true;
		}
		else
		{
			isSetVolume = false;
		}
		testVolume = std::clamp(testVolume, 0.0f, 1.0f);

		if (Input::IsKeyPushed(KeyCode::Button::D4)) crossFadeTime += 0.5f;
		if (Input::IsKeyPushed(KeyCode::Button::D3)) crossFadeTime -= 0.5f;
		crossFadeTime = std::clamp(crossFadeTime, 0.0f, 15.0f);
		if (Input::IsKeyPushed(KeyCode::Button::F)) Sound::CrossfadeBGM(testSound02, false, crossFadeTime);

		if (isSetVolume)
		{
			Sound::SetVolume(testSound02, testVolume);
		}

		std::string playSE{ "G : PlaySE" };
		std::string playPhuniaya{ "1 : PlayBGM(Phuniaya_2)" };
		std::string playBetterDays{ "2 : PlayBGM(BetterDays)" };
		std::string stopBGM{ "S : StopBGM" };
		std::string endBGM{ "Return : EndBGM" };
		std::string upVolume{ "Right : VolumeUp(Phuniaya_2)" };
		std::string downVolume{ "Left : VolumeDown(Phuniaya_2)" };
		std::string currentVolume{ std::format("CurrentVolume(Phuniaya_2) : {:.1f}", testVolume) };
		std::string crossFadeTimePlusGuide{ "4 : CrossFadeTime +0.5" };
		std::string crossFadeTimeMinusGuide{ "3 : CrossFadeTime -0.5" };
		std::string currentCrossFadeTime{ std::format("CrossFadeTime : {:.1f}", crossFadeTime)};
		std::string crossFade{ "F : CrossFade(ToPhuniaya_2)" };

		Gfx::ClearScreen();

		Gfx::DrawString(playSE.c_str(), { 0.0f, 0.0f });
		Gfx::DrawString(playPhuniaya.c_str(), { 0.0f, 30.0f });
		Gfx::DrawString(playBetterDays.c_str(), { 0.0f, 60.0f });
		Gfx::DrawString(stopBGM.c_str(), { 0.0f, 90.0f });
		Gfx::DrawString(endBGM.c_str(), { 0.0f, 120.0f });
		Gfx::DrawString(upVolume.c_str(), { 0.0f, 150.0f });
		Gfx::DrawString(downVolume.c_str(), { 0.0f, 180.0f });
		Gfx::DrawString(currentVolume.c_str(), { 0.0f, 210.0f });
		Gfx::DrawString(crossFadeTimePlusGuide.c_str(), { 0.0f, 240.0f });
		Gfx::DrawString(crossFadeTimeMinusGuide.c_str(), { 0.0f, 270.0f });
		Gfx::DrawString(currentCrossFadeTime.c_str(), { 0.0f, 300.0f });
		Gfx::DrawString(crossFade.c_str(), { 0.0f, 330.0f });

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
