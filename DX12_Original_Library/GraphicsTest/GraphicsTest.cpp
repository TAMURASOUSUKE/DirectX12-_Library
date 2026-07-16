#include "../Src/Facade/TSLib.h"
#include <string> // テスト用
#include "../Src/Graphics/GraphicsType.h" // デバッグ用に一時的に
#include "DescriptorManager.h" // Allocator関数を呼び出しメモリ確保できるかのテスト

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"GraphicsTest", 1280, 720)) return -1;


	DescriptorHandle h1{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視
	DescriptorHandle h2{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // GPU可視

	// h1とh2が別のインデックスであることを確認する
	if (h1.index != h2.index)
	{
		OutputDebugStringA("[PASS] : インデックスが異なる値を出力できています\n");
	}
	else
	{
		OutputDebugStringA("[FAIL] : インデックスが同じ値を出力しています\n");
	}

	// Freeして再度Allocateすると同じインデックスが戻るか
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, h2);
	DescriptorHandle h3{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // 再度取得

	if (h2.index == h3.index)
	{
		OutputDebugStringA("[PASS] : 一度戻した後も同じインデックスが返っています\n");
	}
	else
	{
		OutputDebugStringA("[FAIL] : 一度戻した後違うインデックスが返っています\n");
	}

	// ハンドルの取得
	TexHandle background{ Gfx::LoadTexture("Res/bg.png") }; // 背景のハンドル取得
	TexHandle enemy{ Gfx::LoadTexture("Res/enemy.png") }; // Enemyのハンドル取得
	TexHandle player{ Gfx::LoadTexture("Res/player.png") }; // Playerのハンドル取得
	TexHandle heightMap{ Gfx::LoadTexture("Res/TestVolume.png") }; // ハイトマップ取得
	ModelHandle testModel{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Testモデルのロード
	ModelHandle testPlayer{ Gfx::LoadModel("Res/TestPlayer.glb") }; // Playerモデルのロード
	AnimInstanceData debugAnim{}; // アニメーション用のデータ
	debugAnim.handle = testModel;
	Vector2 playerPos{ 100.0f, 100.0f };
	float ang{ 0.0f }; // 角度加算用のテスト
	Vector3 cubeAng{ Vector3::Zero };

	Transform cubeTransform{};
	Transform cubeTransform02{};
	cubeTransform.SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
	cubeTransform02.SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
	cubeTransform.SetScale(Vector3::One);

	bool testFlag{ false };

	int testWheel{ 0 };
	int testWheelNotch{ 0 };
	float heightFactor{ 0.0f };
	float tessFactor{ 4.0f };

	enum class ActionMap{Jump, Dash, Count}; // 抽象化テスト用アクション
	Input::SetupActions(ActionMap::Count); // 初期化
	Input::SetAction(ActionMap::Jump, KeyCode::Button::SPACE);
	Input::SetAction(ActionMap::Jump, PadCode::Button::A);
	Input::SetAction(ActionMap::Jump, MouseCode::Click::LEFT);
	Input::SetAction(ActionMap::Dash, KeyCode::Button::LSHIFT);
	Input::SetAction(ActionMap::Dash, PadCode::Trigger::RIGHT);
	Input::SetAction(ActionMap::Dash, MouseCode::Click::RIGHT);

	SoundHandle testSound{ Sound::LoadSound("Test.wav") };
	SoundHandle testSound02{ Sound::LoadSound("Phuniaya_2.wav") };
	SoundHandle testSound03{ Sound::LoadSound("Better_Days.wav") };
	float testBolume{ 0.8f };

	while (Gfx::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理

		Vector2 dir{ Input::GetPadStickValue(PadCode::Stick::RIGHT)};

		playerPos += dir * 8.0f;

		ang += 0.01f;

		ang = Math::NormalizeAngle(ang); // 角度の正規化

		cubeAng.x += 0.01f;
		cubeAng.y += 0.01f;
		cubeAng.z += 0.01f;
		cubeAng = Math::NormalizeAngle(cubeAng);

		// アニメーションテスト
		debugAnim.currentTime += 1.0f / 60.0f; // 時刻を進める
		if (debugAnim.currentTime > 0.667f) debugAnim.currentTime = 0.0f; // 一旦Runのdurationでループさせる

		// 音のテスト
		if (Input::IsKeyPushed(KeyCode::Button::G))
		{
			Sound::PlaySE(testSound);
		}
		if (Input::IsKeyPushed(KeyCode::Button::D2))
		{
			Sound::PlayBGM(testSound02, false, testBolume);
		}
		if (Input::IsKeyPushed(KeyCode::Button::D3))
		{
			Sound::PlayBGM(testSound03, true, 0.8f);
		}
		if (Input::IsKeyPushed(KeyCode::Button::S))
		{
			Sound::StopBGM();
		}
		if (Input::IsKeyPushed(KeyCode::Button::RETURN))
		{
			Sound::EndBGM();
		}
		if (Input::IsKeyPress(KeyCode::Button::RIGHT))
		{
			heightFactor += 0.05f;
			testBolume += 0.005f;
			Sound::SetVolume(testSound02, testBolume);
		}
		if (Input::IsKeyPress(KeyCode::Button::LEFT))
		{
			testBolume -= 0.005f;
			heightFactor -= 0.05f;
			Sound::SetVolume(testSound02, testBolume);
		}
		if (Input::IsKeyPushed(KeyCode::Button::F))
		{
			Sound::CrossfadeBGM(testSound02, false, 10.0f);
		}
		if(Input::IsKeyPushed(KeyCode::Button::L))
		{
			tessFactor *= 2.0f;
		}
		if (Input::IsKeyPushed(KeyCode::Button::J))
		{
			tessFactor /= 2.0f;
		}
		if (tessFactor < 2.0f) tessFactor = 2.0f;

		Gfx::ClearScreen(); // 画面クリア(黒)

		//// スプライトバッチテスト
		Gfx::DrawSprite(background, Vector2{ 0.0f, 0.0f }, Vector2{ 1280.0f, 720.0f }, 0.0f, Vector2::Zero, Vector2::One, LenderLayer::BackGround);
		Gfx::DrawSprite(player, playerPos, Vector2{128.0f, 128.0f});

		TexHandle test{};
		if (Input::IsActionPushed(ActionMap::Dash)) testFlag = !testFlag;
		if (testFlag)
		{
			test = player;
		}
		else
		{
			test = enemy;
		}

		Gfx::DrawSprite(test, Vector2{ 800.0f, 400.0f }, Vector2{ 128.0f, 128.0f });

		float inputTrigger{ Input::GetPadTriggerValue(PadCode::Trigger::RIGHT) };
		std::string triggerStr{ std::format("Input Trigger Value : {:.2f}", inputTrigger) };
		Vector2Int cursorPos{ Input::GetMousePoint() };
		std::string cursorStr{ std::format("Input CursorPosition x : {},  y : {}", cursorPos.x, cursorPos.y) };
		Vector2Int cursorDelta{ Input::GetMouseDelta() };
		std::string cursorDeltaStr{ std::format("Input CursorDelta x : {},  y : {}", cursorDelta.x, cursorDelta.y) };
		testWheel += Input::GetMouseWheelValue();
		std::string wheelValueStr{ std::format("Input Wheel Value : {}", testWheel) };
		testWheelNotch += Input::GetMouseWheelNotchValue();
		std::string wheelNotchValueStr{ std::format("Input WheelNotch Value : {}", testWheelNotch) };
		Gfx::DrawString(triggerStr.c_str(), {0.0f, 0.0f});
		Gfx::DrawString(cursorStr.c_str(), {0.0f, 30.0f});
		Gfx::DrawString(cursorDeltaStr.c_str(), {0.0f, 60.0f});
		Gfx::DrawString(wheelValueStr.c_str(), {0.0f, 90.0f});
		Gfx::DrawString(wheelNotchValueStr.c_str(), {0.0f, 120.0f});

		Gfx::DrawTerrain({ 0.0f, -10.0f, 20.0f }, 30.0f, tessFactor, heightFactor, { 1.0f, 0.0f, 0.0f, 0.0f }, heightMap);

		 Gfx::SetTexture(testModel, 0, enemy);

		 Gfx::Unload(enemy);

		Gfx::DrawModel(testModel, cubeTransform, &debugAnim);

		Gfx::DrawSprite(enemy, {500.0f, 500.0f}, Vector2{ 128.0f, 128.0f });

		Gfx::DrawCapsule({30.0f, 30.0f}, {30.0f, 200.0f}, 40.0f, {1.0f, 1.0f, 1.0f, 1.0f}, true);
		Gfx::DrawCapsule({120.0f, 80.0f}, {120.0f, 200.0f}, 40.0f, { 0.0f, 1.0f, 0.0f, 1.0f }, true);
		Gfx::DrawCircle({ 120.0f, 300.0f }, 30.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
		Gfx::DrawCircle({ 800.0f, 300.0f }, 30.0f, { 1.0f, 0.0f, 1.0f, 1.0f });

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}