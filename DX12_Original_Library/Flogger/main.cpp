#include <string>
#include "../Src/Facade/TSLib.h"
#include "Objects/ObjectManager.h"
#include "Objects/ObjectFactory.h" // 本当はもっとしっかり分けたいが規模的にいったんこの形

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"Flogger", 1280, 720)) return -1;

	/*
		規模が小さいのでmainでスコアとlifeを管理しています
	*/
	int score{ 0 };
	int playerLife{ 3 };
	TexHandle background{ Gfx::LoadTexture("Res/BG.png")}; // 背景ロード
	TexHandle redCar{ Gfx::LoadTexture("Res/RedCar.png")}; // 赤い車ロード
	TexHandle minivan{ Gfx::LoadTexture("Res/Minivan.png") }; // ミニバンロード
	TexHandle trailer{ Gfx::LoadTexture("Res/Trailer.png") }; // トレーラー
	TexHandle player{ Gfx::LoadTexture("Res/Player.png") }; // プレイヤー

	ObjectFactory::CreateBackground(background, Vector2::Zero, {1280.0f, 720.0f}, 0.0f);
	ObjectFactory::CreatePlayer(player, { 629.0f, 632.0f }, { 42.0f, 88.0f }, 0.0f, 200.0f,
		[&score]()
		{
			// ゴール時
			++score;
		},
		[&playerLife](int _newScore)
		{
			// life変動
			playerLife = _newScore;
		}
	);
	ObjectFactory::CreateCar(redCar, { -88.0f, 240.0f }, {88.0f, 46.0f}, 0.0f, 400.0f);
	ObjectFactory::CreateCar(trailer, {1280.0f + 352.0f, 314.0f}, {352.0f, 88.0f}, 180.0f * Math::DEG_TO_RAD, -100.0f);
	ObjectFactory::CreateCar(minivan, { 1280.0f + 176.0f, 122.0f }, { 176.0f, 88.0f }, 180.0f * Math::DEG_TO_RAD, -200.0f);

	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理
		
		ObjectManager::Instance().Update();
		const std::string scoreContext{ "Score : " + std::to_string(score) };
		const std::string lifeContext{ "Rest : " + std::to_string(playerLife) };
		
		Gfx::ClearScreen(); // 画面クリア

		ObjectManager::Instance().Draw();
		Gfx::DrawString(scoreContext.c_str(), { 256.0f, 40.0f });
		Gfx::DrawString(lifeContext.c_str(), { 1024.0f, 40.0f });

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}