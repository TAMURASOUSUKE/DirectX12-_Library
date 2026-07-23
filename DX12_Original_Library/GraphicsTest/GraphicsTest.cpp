#include "../Src/Facade/TSLib.h"
#include <string> // テスト用
#include <algorithm>
#include "../Src/Graphics/GraphicsType.h" // デバッグ用に一時的に

// テスト用として渡す定数バッファ
struct GrayScaleParameter
{
	float strength{ 1.0f };
	float padding[3]{}; // 16byteに合うように対策
};

// 色の補正を掛けるCB(複数のパラメータを渡せるかのテスト)
struct ColorOffsetParameter
{
	float red{ 0.0f };
	float green{ 0.0f };
	float blue{ 0.0f };
	float padding{ 0.0f };
};

// Sprite外部テスト
struct InverseParameter
{
	float strength{ 0.0f };
	float padding[3]{};
};

// Spriteが複数のパラメータの影響を受けることができるかのテスト
struct GlitchParameter
{
	float time{ 0.0f };
	float strength{ 1.0f };
	float chromaticOffset{ 0.008f };
	float scanlineCount{ 80.0f };
};

struct GlitchColorParameter
{
	float red{ 0.0f };
	float green{ 1.0f };
	float blue{ 1.0f };
	float amount{ 0.35f };
};

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const Vector2 windowSize{ 1980.0f, 1080.0f };

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"GraphicsTest", static_cast<int>(windowSize.x), static_cast<int>(windowSize.y))) return -1;
	Time::SetTargetFPS(0);

	// ハンドルの取得
	// PostEffect
	ShaderHandle grayScalePS{ Gfx::LoadShader(L"Shaders/GrayScalePS.hlsl", ShaderUsage::PostEffect, ShaderStage::Pixel) };
	MaterialHandle grayScaleMaterial{ Gfx::CreateMaterial(grayScalePS) };
	GrayScaleParameter grayScaleParam{};
	ColorOffsetParameter colorOffsetParam{};

	// SpriteShader
	ShaderHandle inverseSpritePS{ Gfx::LoadShader(L"Shaders/InverseSpritePS.hlsl", ShaderUsage::Sprite, ShaderStage::Pixel)};
	MaterialHandle inverseSpriteMaterial{ Gfx::CreateMaterial(inverseSpritePS) };
	InverseParameter inverseParam{};
	ShaderHandle glitchPS{ Gfx::LoadShader(L"Shaders/GlitchSpritePS.hlsl", ShaderUsage::Sprite, ShaderStage::Pixel) };
	MaterialHandle glitchMaterial{ Gfx::CreateMaterial(glitchPS) };
	GlitchParameter glitch{};
	GlitchColorParameter glitchColor{};
	Gfx::SetMaterialParameter(glitchMaterial, 1, glitchColor);
	//Gfx::Unload(inverseSpriteMaterial);
	//inverseSpriteMaterial = {};

	// Texture
	TexHandle background{ Gfx::LoadTexture("Res/bg.png") }; // 背景のハンドル取得
	TexHandle enemy{ Gfx::LoadTexture("Res/enemy.png") }; // Enemyのハンドル取得
	TexHandle player{ Gfx::LoadTexture("Res/player.png") }; // Playerのハンドル取得
	TexHandle heightMap{ Gfx::LoadTexture("Res/TestVolume.png") }; // ハイトマップ取得
	TexHandle minivan{ Gfx::LoadTexture("Res/Minivan.png") }; // sRGBテスト
	// TexHandle heightMap{ Gfx::LoadTexture("Res/Crater.jpg") }; // ハイトマップ取得

	// Model
	ModelHandle testModel{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Testモデルのロード
	ModelHandle testPlayer{ Gfx::LoadModel("Res/TestPlayer.glb") }; // Playerモデルのロード
	AnimInstanceData debugAnim{}; // アニメーション用のデータ
	debugAnim.handle = testModel;
	Vector2 enemyPos{ 100.0f, 100.0f };

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
	float time{ 0.0f };

	// ゲームループ
	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理
		time += Time::DeltaTime();
		glitch.time += Time::DeltaTime();

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

		float moveSpeed{ 300.0f }; // 1秒間に移動するピクセル
		testRect01.position += dir * moveSpeed * Time::DeltaTime();
		// シェーダーを掛けたSpriteの挙動も見たのでそっちも動かす
		enemyPos += dir * moveSpeed * Time::DeltaTime();

		if (Collision::Intersect(testRect01, testRect02)) debugColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		else debugColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		// PostEffect操作
		float rate{ 0.5f }; // パラメータを動かす速度
		if(Input::IsKeyPushed(KeyCode::Button::D5)) Gfx::SetPostEffect(grayScaleMaterial); // グレースケール変更
		if (Input::IsKeyPushed(KeyCode::Button::D4)) Gfx::SetPostEffect({}); // 内蔵へ戻す
		if (Input::IsKeyPress(KeyCode::Button::D7)) grayScaleParam.strength -= rate * Time::UnscaledDeltaTime();
		if (Input::IsKeyPress(KeyCode::Button::D8)) grayScaleParam.strength += rate * Time::UnscaledDeltaTime();
		if (Input::IsKeyPress(KeyCode::Button::R)) colorOffsetParam.red = 0.5f - 0.5f * std::sinf(time);
		if (Input::IsKeyPress(KeyCode::Button::G)) colorOffsetParam.green = 0.5f - 0.5f * std::sinf(time);
		if (Input::IsKeyPress(KeyCode::Button::B)) colorOffsetParam.blue = 0.5f - 0.5f * std::sinf(time);
		grayScaleParam.strength = std::clamp(grayScaleParam.strength, 0.0f, 1.0f);
		Gfx::SetMaterialParameter(grayScaleMaterial, grayScaleParam);
		Gfx::SetMaterialParameter(grayScaleMaterial, 1, colorOffsetParam);

		// Sprite操作
		float glitchSpeed{ 3.0f };
		if (Input::IsKeyPress(KeyCode::Button::D9)) inverseParam.strength = 0.5f + 0.5f * std::sinf(time);
		if (Input::IsKeyPress(KeyCode::Button::L)) glitch.strength = 1.0f;
		if (Input::IsKeyPress(KeyCode::Button::J)) glitch.strength = 0.0f;
		Gfx::SetMaterialParameter(inverseSpriteMaterial, inverseParam);
		Gfx::SetMaterialParameter(glitchMaterial, 0, glitch);

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
		Gfx::DrawSprite(background, { 0.0f, 0.0f }, windowSize, 0.0f, Vector4::One, Vector2::Zero, Vector2::One, LenderLayer::BackGround);
		Gfx::DrawSprite(enemy, { 350.0f, 350.0f }, { 128.0f, 128.0f });

		Gfx::DrawTerrain({ 0.0f, -10.0f, 20.0f }, 80.0f, tessFactor, heightFactor, { 1.0f, 0.0f, 0.0f, 0.0f }, heightMap);

		Gfx::DrawModel(testModel, cubeTransform, &debugAnim);

		Gfx::DrawSprite(enemy, {500.0f, 500.0f}, { 128.0f, 128.0f });
		Gfx::DrawSprite(minivan, { 800.0f, 500.0f }, { 176.0f, 88.0f });

		// Shaderテスト
		Gfx::DrawSprite(enemy, { 100.0f, 300.0f }, { 128.0f, 128.0f }, 0.0f, {1.0f, 0.0f, 0.0f, 1.0f});

		Gfx::DrawSprite(minivan, { 300.0f, 300.0f }, { 176.0f, 88.0f });

		Gfx::DrawSprite(enemy,{ 500.0f, 300.0f }, { 128.0f, 128.0f }, inverseSpriteMaterial);

		Gfx::DrawSprite(minivan, { 800.0f, 300.0f }, { 176.0f, 88.0f }, inverseSpriteMaterial, 0.0f, { 1.0f, 1.0f, 1.0f, 0.5f + 0.5f * sinf(time) });

		Gfx::DrawSprite(enemy, { 900.0f, 300.0f }, { 128.0f, 128.0f }, 0.0f, { 1.0f, 1.0f, 1.0f, 0.5f });

		Gfx::DrawSprite(enemy, enemyPos, { 128.0f, 128.0f }, glitchMaterial);

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
