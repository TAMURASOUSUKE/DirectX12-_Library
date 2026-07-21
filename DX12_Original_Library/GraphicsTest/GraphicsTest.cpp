#include "../Src/Facade/TSLib.h"
#include <string> // テスト用
#include <algorithm>
#include "../Src/Graphics/GraphicsType.h" // デバッグ用に一時的に
#include "DescriptorManager.h" // Allocator関数を呼び出しメモリ確保できるかのテスト

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"GraphicsTest", 1280, 720)) return -1;
	Time::SetTargetFPS(0);

	ShaderHandle grayScalePS{ Gfx::LoadShader(L"Shaders/GrayScalePS.hlsl", ShaderUsage::PostEffect, ShaderStage::Pixel) };
	if (grayScalePS.IsValid())
	{
		DEBUG_LOG("[PASS] 外部PixelShaderの読み込みに成功しました\n");
	}
	else
	{
		DEBUG_LOG("[FAIL] 外部PixelShaderの読み込みに失敗しました\n");
	}
	MaterialHandle grayScaleMaterial{ Gfx::CreateMaterial(grayScalePS) };
	if (grayScaleMaterial.IsValid())
	{
			DEBUG_LOG("[PASS] Material用のPSOの作成に成功しました\n");
	}
	else
	{
		DEBUG_LOG("[FAIL] Material用のPSOの作成に失敗しました\n");
	}

	// ハンドルの取得
	TexHandle background{ Gfx::LoadTexture("Res/bg.png") }; // 背景のハンドル取得
	TexHandle enemy{ Gfx::LoadTexture("Res/enemy.png") }; // Enemyのハンドル取得
	TexHandle player{ Gfx::LoadTexture("Res/player.png") }; // Playerのハンドル取得
	TexHandle heightMap{ Gfx::LoadTexture("Res/TestVolume.png") }; // ハイトマップ取得
	// TexHandle heightMap{ Gfx::LoadTexture("Res/Crater.jpg") }; // ハイトマップ取得
	ModelHandle testModel{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Testモデルのロード
	ModelHandle testPlayer{ Gfx::LoadModel("Res/TestPlayer.glb") }; // Playerモデルのロード
	TexHandle minivan{ Gfx::LoadTexture("Res/Minivan.png") }; // sRGBテスト
	AnimInstanceData debugAnim{}; // アニメーション用のデータ
	debugAnim.handle = testModel;
	Vector2 playerPos{ 100.0f, 100.0f };

	Transform cubeTransform{};
	Transform cubeTransform02{};
	cubeTransform.SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
	cubeTransform02.SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
	cubeTransform.SetScale(Vector3::One);

	bool testFlag{ false };
	int testWheel{ 0 };
	int testWheelNotch{ 0 };

	float t{ 0.0f }; // 時間
	float tessFactor{ 4.0f }; // HSでの分割数
	float heightFactor{ 0.0f }; // Terrainの高さ

	// あたり判定(テスト)
	Rect testRect01{ {200.0f, 200.0f}, {30.0f, 30.0f} };
	Rect testRect02{ {400.0f, 400.0f}, {30.0f, 30.0f} };
	Vector4 debugColor{ 1.0f, 1.0f, 1.0f, 1.0f };

	// タイムスケール
	float timeScale{ 1.0f };

	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理

		// アニメーションテスト
		debugAnim.currentTime += Time::DeltaTime(); // 時刻を進める
		if (debugAnim.currentTime > 0.667f) debugAnim.currentTime = 0.0f; // 一旦Runのdurationでループさせる

		// Terrain操作
		float heightSpeed{ 3.0f };
		if (Input::IsKeyPress(KeyCode::Button::UP)) heightFactor += heightSpeed * Time::UnscaledDeltaTime();
		if (Input::IsKeyPress(KeyCode::Button::DOWN)) heightFactor -= heightSpeed * Time::UnscaledDeltaTime();
		if(Input::IsKeyPushed(KeyCode::Button::D2)) tessFactor *= 2.0f;
		if (Input::IsKeyPushed(KeyCode::Button::D1)) tessFactor /= 2.0f;
		tessFactor = std::clamp(tessFactor, 2.0f, 64.0f);

		heightFactor = std::clamp(heightFactor, -20.0f, 20.0f);

		// 矩形のあたり判定確認操作
		Vector2 dir{ Vector2::Zero };
		if (Input::IsKeyPress(KeyCode::Button::W)) dir.y -= 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::A)) dir.x -= 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::S)) dir.y += 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::D)) dir.x += 1.0f;
		dir.Normalize();

		float moveSpeed{ 80.0f }; // 1秒間に移動するピクセル
		testRect01.position += dir * moveSpeed * Time::DeltaTime();

		if (Collision::Intersect(testRect01, testRect02)) debugColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		else debugColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		// PostEffect操作
		if(Input::IsKeyPushed(KeyCode::Button::D4)) Gfx::SetPostEffect(grayScaleMaterial); // グレースケール変更
		if (Input::IsKeyPushed(KeyCode::Button::D5)) Gfx::SetPostEffect({}); // 内蔵へ戻す

		// FPS操作
		if (Input::IsKeyPushed(KeyCode::Button::D3)) Time::SetTargetFPS(30); // 30FPS
		if (Input::IsKeyPushed(KeyCode::Button::D6)) Time::SetTargetFPS(60); // 60FPS
		if (Input::IsKeyPushed(KeyCode::Button::D0)) Time::SetTargetFPS(120); // 120FPS モニターが120Hz以上である必要あり
		if (Input::IsKeyPushed(KeyCode::Button::RIGHT)) timeScale += 1.0f;
		if (Input::IsKeyPushed(KeyCode::Button::LEFT)) timeScale -= 1.0f;
		timeScale =  std::clamp(timeScale, 0.0f, 10.0f); // 最大でもタイムスケールは10にとどめておく
		Time::SetTimeScale(timeScale);


		std::string fpsValue{ std::format("CurrentMeasuredFPS : {:.1f}", Time::FPS()) };
		std::string targetFPS{ std::format("CurrentSettingFPS : {}", Time::GetTargetFPS()) };
		std::string unscaledDeltaTime{ std::format("CurrentUnscaledDeltaTime: {:.6f}", Time::UnscaledDeltaTime()) };
		std::string deltaTime{ std::format("CurrentDeltaTime : {:.3f}", Time::DeltaTime()) };
		std::string timeScale{ std::format("CurrentTimeScale : {:.2f}", Time::GetTimeScale()) };

		Gfx::ClearScreen(); // 画面クリア(黒)

		// スプライトバッチテスト
		Gfx::DrawSprite(background, { 0.0f, 0.0f }, { 1280.0f, 720.0f }, 0.0f, Vector2::Zero, Vector2::One, LenderLayer::BackGround);
		Gfx::DrawSprite(enemy, { 350.0f, 350.0f }, { 128.0f, 128.0f });

		Gfx::DrawTerrain({ 0.0f, -10.0f, 20.0f }, 80.0f, tessFactor, heightFactor, { 1.0f, 0.0f, 0.0f, 0.0f }, heightMap);

		Gfx::DrawModel(testModel, cubeTransform, &debugAnim);

		Gfx::DrawSprite(enemy, {500.0f, 500.0f}, { 128.0f, 128.0f });
		Gfx::DrawSprite(minivan, { 800.0f, 500.0f }, { 176.0f, 88.0f });

		//Gfx::DrawCapsule({30.0f, 30.0f}, {30.0f, 200.0f}, 40.0f, {1.0f, 1.0f, 1.0f, 1.0f}, true);
		//Gfx::DrawCapsule({120.0f, 80.0f}, {120.0f, 200.0f}, 40.0f, { 0.0f, 1.0f, 0.0f, 1.0f }, true);
		//Gfx::DrawCircle({ 120.0f, 300.0f }, 30.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
		//Gfx::DrawCircle({ std::sinf(t) * 50.0f + 600.0f, 300.0f}, 30.0f, {std::clamp(std::sinf(t), 0.0f, 1.0f), 0.0f, 0.0f, 1.0f});

		Gfx::DrawBox(testRect01.GetMinPos(), testRect01.GetMaxPos());
		Gfx::DrawBox(testRect02.GetMinPos(), testRect02.GetMaxPos(), 0.0f, debugColor);
		Gfx::DrawString(fpsValue.c_str(), {0.0f, 0.0f});
		Gfx::DrawString(targetFPS.c_str(), {0.0f, 30.0f});
		Gfx::DrawString(unscaledDeltaTime.c_str(), {0.0f, 60.0f});
		Gfx::DrawString(deltaTime.c_str(), {0.0f, 90.0f});
		Gfx::DrawString(timeScale.c_str(), {0.0f, 120.0f});

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}