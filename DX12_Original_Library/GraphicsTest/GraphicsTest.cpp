#include "../Src/Facade/TSLib.h"
#include <string> // テスト用
#include <algorithm>
#include "../Src/Graphics/GraphicsType.h" // デバッグ用に一時的に
#include "../Src/Facade/GfxInternal.h" // デバッグ用に一時的に

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

// 現在のグリッチをアトラス対応させるもの
struct AtlasParameter
{
	Vector4 atlasUVRect;
	float bandCount{ 24.0f }; // 作る帯の数
	float padding[3]; 
};

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	const Vector2 windowSize{ 1280.0f, 720.0f };

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"GraphicsTest", static_cast<int>(windowSize.x), static_cast<int>(windowSize.y)))return -1;
	Time::SetTargetFPS(0);

	// ハンドルの取得
	// PostEffect
	ShaderHandle grayScalePS{ Gfx::LoadShader(L"Shaders/GrayScalePS.hlsl", ShaderUsage::PostEffect, ShaderStage::Pixel) };
	MaterialHandle grayScaleMaterial{ Gfx::CreateMaterial(grayScalePS) };
	GrayScaleParameter grayScaleParam{};
	ColorOffsetParameter colorOffsetParam{};

	// SpriteShader
	ShaderHandle inverseSpritePS{ Gfx::LoadShader(L"Shaders/InverseSpritePS.hlsl", ShaderUsage::Sprite, ShaderStage::Pixel) };
	MaterialHandle inverseSpriteMaterial{ Gfx::CreateMaterial(inverseSpritePS) };
	InverseParameter inverseParam{};
	ShaderHandle glitchPS{ Gfx::LoadShader(L"Shaders/GlitchSpritePS.hlsl", ShaderUsage::Sprite, ShaderStage::Pixel) };
	MaterialHandle glitchMaterial{ Gfx::CreateMaterial(glitchPS) };
	GlitchParameter glitch{};
	GlitchColorParameter glitchColor{};
	AtlasParameter atlasParameter{};
	AtlasParameter fullParameter{ {0.0f, 0.0f, 1.0f, 1.0f }, 24.0f }; // アトラスではないもの用
	Gfx::SetMaterialParameter(glitchMaterial, 1, glitchColor);
	//Gfx::Unload(inverseSpriteMaterial);
	//inverseSpriteMaterial = {};

	// Texture
	Vector2 enemyPos{ 100.0f, 100.0f };
	TexHandle background{ Gfx::LoadTexture("Res/bg.png") }; // 背景のハンドル取得
	TexHandle enemy{ Gfx::LoadTexture("Res/enemy.png") }; // Enemyのハンドル取得
	TexHandle player{ Gfx::LoadTexture("Res/player.png") }; // Playerのハンドル取得
	TexHandle heightMap{ Gfx::LoadTexture("Res/TestVolume.png") }; // ハイトマップ取得
	TexHandle minivan{ Gfx::LoadTexture("Res/Minivan.png") }; // sRGBテスト
	TexHandle runtimeTexture{}; // 実行中にロードができるか確認
	bool hasLoadedRuntimeTexture{ false };
	// TexHandle heightMap{ Gfx::LoadTexture("Res/Crater.jpg") }; // ハイトマップ取得

	// Atlas
	Gfx::TextureAtlas idleAnim{ Gfx::LoadTextureAtlas("Res/Idle.png", 8, 1, 8) }; // IdleMotion
	Gfx::TextureAtlas runAnim{ Gfx::LoadTextureAtlas("Res/Run.png", 8, 1, 8) }; // RunMotion
	Gfx::TextureAtlas attack01Anim{ Gfx::LoadTextureAtlas("Res/Attack1.png", 8, 1, 8) }; // Attack01Motion
	Gfx::TextureAtlas attack02Anim{ Gfx::LoadTextureAtlas("Res/Attack2.png", 8, 1, 8) }; // Attack01Motion
	Gfx::TextureAtlas JumpAnim{ Gfx::LoadTextureAtlas("Res/Jump.png", 2, 1, 2) }; // JumpMotion
	Gfx::TextureAtlas FallAnim{ Gfx::LoadTextureAtlas("Res/Fall.png", 2, 1, 2) }; // FallMotion
	// アトラスをまとめた配列
	Gfx::TextureAtlas atlasAnims[]{ idleAnim, runAnim, attack01Anim, attack02Anim, JumpAnim, FallAnim };
	// アトラス配列と順をそろえる
	Gfx::SpriteAnimationState atlasAnimDescs[]{ {0, idleAnim.frameCount - 1,0.1f, true}, {0, runAnim.frameCount - 1, 0.1f, true}, {0, attack01Anim.frameCount - 1, 0.1f, false}, {0, JumpAnim.frameCount - 1, 0.1f, true} }; 
	Gfx::SpriteFlip flip{ Gfx::SpriteFlip::None };
	int atlasAnimIndex{ 0 };
	int atlasDescIndex{ 0 };
	bool isAttacking{ false };
	int attackAtlasIndex{ 2 };

	// Model
	ModelHandle testModel{ Gfx::LoadModel("Res/TestMultipleAnimModel.glb") }; // Testモデルのロード
	ModelHandle testPlayer{ Gfx::LoadModel("Res/TestPlayer.glb") }; // Playerモデルのロード
	Transform modelTransform01{};
	Transform modelTransform02{};
	modelTransform01.SetPosition({ -1.0f, 0.0f, 0.0f });
	modelTransform02.SetPosition({ 1.0f, 0.0f, 0.0f });
	AnimInstanceData anim01{};
	AnimInstanceData anim02{};
	anim01.handle = testModel;
	anim02.handle = testModel;

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
	constexpr float ANIM_FRAME_DURATION{ 1.0f / 10.0f };
	float timeScale{ 1.0f };
	float time{ 0.0f };
	int spriteAnimTime{ 0 };


	// ゲームループ
	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理
		time += Time::DeltaTime();
		glitch.time += Time::DeltaTime();

		if (Input::IsKeyPushed(KeyCode::Button::TAB) && !hasLoadedRuntimeTexture)
		{
			runtimeTexture = Gfx::LoadTexture("Res/T_003_sword_01_01.png");
			hasLoadedRuntimeTexture = runtimeTexture.IsValid();
		}

		// アニメーションテスト
		anim01.currentTime += Time::DeltaTime();
		anim02.currentTime += Time::DeltaTime() * 0.5f;
		if (anim01.currentTime > 0.667f) anim01.currentTime = 0.0f; // 一旦Runのdurationでループさせる
		if (anim02.currentTime > 0.667f) anim02.currentTime = 0.0f; // 一旦Runのdurationでループさせる

		// Terrain操作
		float heightSpeed{ 3.0f };
		if (Input::IsKeyPress(KeyCode::Button::UP)) heightFactor += heightSpeed * Time::UnscaledDeltaTime();
		if (Input::IsKeyPress(KeyCode::Button::DOWN)) heightFactor -= heightSpeed * Time::UnscaledDeltaTime();
		if (Input::IsKeyPushed(KeyCode::Button::D2)) tessFactor *= 2.0f;
		if (Input::IsKeyPushed(KeyCode::Button::D1)) tessFactor /= 2.0f;
		tessFactor = std::clamp(tessFactor, 2.0f, 64.0f);

		heightFactor = std::clamp(heightFactor, -20.0f, 20.0f);

		// 操作(矩形判定確認やキャラクター動作確認に使っています)
		Vector2 dir{ Vector2::Zero };
		if (Input::IsKeyPress(KeyCode::Button::W))
		{
			dir.y -= 1.0f;
		}
		if (Input::IsKeyPress(KeyCode::Button::A)) 
		{ 
			// 移動と反転
			dir.x -= 1.0f;  
			if(!isAttacking && Time::GetTimeScale() != 0.0f) flip = Gfx::SpriteFlip::Horizontal; 
		}
		if (Input::IsKeyPress(KeyCode::Button::S))
		{
			dir.y += 1.0f;
		}
		if (Input::IsKeyPress(KeyCode::Button::D)) 
		{
			// 移動と反転
			dir.x += 1.0f;
			if (!isAttacking && Time::GetTimeScale() != 0.0f) flip = Gfx::SpriteFlip::None;
		}
		dir.Normalize();

		// 攻撃アニメーション
		constexpr int ATTACK_INDEX{ 2 };
		if (!isAttacking && Input::IsMousePushed(MouseCode::Click::LEFT) && Time::GetTimeScale() != 0.0f)
		{
			isAttacking = true;
			attackAtlasIndex = 2;
			atlasAnimDescs[ATTACK_INDEX].Reset();
		}
		if (!isAttacking && Input::IsMousePushed(MouseCode::Click::RIGHT) && Time::GetTimeScale() != 0.0f)
		{
			isAttacking = true;
			attackAtlasIndex = 3;
			atlasAnimDescs[ATTACK_INDEX].Reset();
		}

		// 攻撃->移動->Idleアニメーション
		if (isAttacking)
		{
			atlasAnimIndex = attackAtlasIndex;
			atlasDescIndex = 2;
			dir = Vector2::Zero;
		}
		else if (dir != Vector2::Zero)
		{
			atlasAnimIndex = 1;
			atlasDescIndex = 1;
		}
		else
		{
			if (Time::GetTimeScale() != 0.0f)
			{
				atlasAnimIndex = 0;
				atlasDescIndex = 0;
			}
		}


		float moveSpeed{ 300.0f }; // 1秒間に移動するピクセル
		testRect01.position += dir * moveSpeed * Time::DeltaTime();
		// シェーダーを掛けたSpriteの挙動も見たのでそっちも動かす
		enemyPos += dir * moveSpeed * Time::DeltaTime();

		if (Collision::Intersect(testRect01, testRect02)) debugColor = { 1.0f, 0.0f, 0.0f, 1.0f };
		else debugColor = { 1.0f, 1.0f, 1.0f, 1.0f };

		// PostEffect操作
		float rate{ 0.5f }; // パラメータを動かす速度
		if (Input::IsKeyPushed(KeyCode::Button::D5)) Gfx::SetPostEffect(grayScaleMaterial); // グレースケール変更
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

		// 2Dアニメーション更新
		Gfx::UpdateSpriteAnimation(atlasAnims[atlasAnimIndex], atlasAnimDescs[atlasDescIndex], Time::DeltaTime());
		if (isAttacking && atlasAnimDescs[ATTACK_INDEX].isFinished) isAttacking = false;

		// 現在描画するアトラスとフレーム(シェーダー側で使うため計算)
		const Gfx::TextureAtlas& currentAtlas{ atlasAnims[atlasAnimIndex] };
		const int frameIndex{ atlasAnimDescs[atlasDescIndex].GetFrameIndex() };
		// 一次元のフレーム番号を列・行へ変換
		const int column{ frameIndex % currentAtlas.columns };
		const int row{ frameIndex / currentAtlas.columns };
		// 1セル分のUV幅
		const float cellWidth{ 1.0f / static_cast<float>(currentAtlas.columns) };
		const float cellHeight{ 1.0f / static_cast<float>(currentAtlas.rows) };
		AtlasParameter atlasParameter{};
		// xyが左上、zwが右下
		atlasParameter.atlasUVRect = { column * cellWidth, row * cellHeight, (column + 1) * cellWidth, (row + 1) * cellHeight };
		atlasParameter.bandCount = 128.0f;
		

		// FPS操作
		if (Input::IsKeyPushed(KeyCode::Button::D3)) Time::SetTargetFPS(30); // 30FPS
		if (Input::IsKeyPushed(KeyCode::Button::D6)) Time::SetTargetFPS(60); // 60FPS
		if (Input::IsKeyPushed(KeyCode::Button::D0)) Time::SetTargetFPS(120); // 120FPS モニターが120Hz以上である必要あり
		if (Input::IsKeyPushed(KeyCode::Button::RIGHT)) timeScale += 1.0f;
		if (Input::IsKeyPushed(KeyCode::Button::LEFT)) timeScale -= 1.0f;
		timeScale = std::clamp(timeScale, 0.0f, 10.0f); // 最大でもタイムスケールは10にとどめておく
		Time::SetTimeScale(timeScale);


		std::string fpsValue{ std::format("CurrentMeasuredFPS : {:.1f}", Time::FPS()) };
		std::string targetFPS{ std::format("CurrentSettingFPS : {}", Time::GetTargetFPS()) };
		std::string unscaledDeltaTime{ std::format("CurrentUnscaledDeltaTime: {:.6f}", Time::UnscaledDeltaTime()) };
		std::string deltaTime{ std::format("CurrentDeltaTime : {:.3f}", Time::DeltaTime()) };
		std::string timeScale{ std::format("CurrentTimeScale : {:.2f}", Time::GetTimeScale()) };

		Gfx::ClearScreen(); // 画面クリア(黒)

		// スプライトバッチテスト
		Gfx::DrawSpriteSized(background, { 0.0f, 0.0f }, windowSize, 0.0f, Gfx::SpriteFlip::None, Vector4::One, Vector2::Zero, Vector2::One, RenderLayer::BackGround);

		Gfx::DrawTerrain({ 0.0f, -10.0f, 20.0f }, 80.0f, tessFactor, heightFactor, { 1.0f, 0.0f, 0.0f, 0.0f }, heightMap);

		// モデル + アニメーション(デバッグ用なのでまだ公開関数ではないです)
		GfxInternal::DrawAnimationModel(modelTransform01, anim01);
		GfxInternal::DrawAnimationModel(modelTransform02, anim02);

		// Shaderテスト
		Gfx::DrawSprite(enemy, { 100.0f, 300.0f }, Vector2::One, 0.0f, Gfx::SpriteFlip::None, { 1.0f, 0.0f, 0.0f, 1.0f });

		Gfx::DrawSprite(minivan, { 300.0f, 300.0f });

		Gfx::DrawSprite(enemy, { 500.0f, 300.0f }, inverseSpriteMaterial);

		Gfx::DrawSprite(minivan, { 800.0f, 300.0f }, inverseSpriteMaterial, Vector2::One, 0.0f, Gfx::SpriteFlip::None, { 1.0f, 1.0f, 1.0f, 0.5f + 0.5f * sinf(time) });

		Gfx::DrawSprite(enemy, { 900.0f, 300.0f }, Vector2::One, 0.0f, Gfx::SpriteFlip::Vertical, { 1.0f, 1.0f, 1.0f, 0.5f });

		Gfx::SetMaterialParameter(glitchMaterial, 2, fullParameter);
		Gfx::DrawSprite(enemy, { 900.0f, 300.0f }, glitchMaterial, { 1.0f, 1.0f }, 0.0f, Gfx::SpriteFlip::Horizontal);

		if (runtimeTexture.IsValid()) Gfx::DrawSprite(runtimeTexture, { 500.0f, 300.0f });

		// アニメーションテスト
		Gfx::SetMaterialParameter(glitchMaterial, 2, atlasParameter);
		Gfx::DrawSprite(atlasAnims[atlasAnimIndex], atlasAnimDescs[atlasDescIndex].GetFrameIndex(), enemyPos, glitchMaterial, { 3.0f, 3.0f }, 0.0f, flip);

		//Gfx::DrawCapsule({30.0f, 30.0f}, {30.0f, 200.0f}, 40.0f, {1.0f, 1.0f, 1.0f, 1.0f}, true);
		//Gfx::DrawCapsule({120.0f, 80.0f}, {120.0f, 200.0f}, 40.0f, { 0.0f, 1.0f, 0.0f, 1.0f }, true);
		//Gfx::DrawCircle({ 120.0f, 300.0f }, 30.0f, { 1.0f, 0.0f, 1.0f, 1.0f });
		//Gfx::DrawCircle({ std::sinf(t) * 50.0f + 600.0f, 300.0f}, 30.0f, {std::clamp(std::sinf(t), 0.0f, 1.0f), 0.0f, 0.0f, 1.0f});

		//Gfx::DrawBox(testRect01.GetMinPos(), testRect01.GetMaxPos());
		//Gfx::DrawBox(testRect02.GetMinPos(), testRect02.GetMaxPos(), 0.0f, debugColor);
		Gfx::DrawString(fpsValue.c_str(), { 0.0f, 0.0f });
		Gfx::DrawString(targetFPS.c_str(), { 0.0f, 30.0f });
		Gfx::DrawString(unscaledDeltaTime.c_str(), { 0.0f, 60.0f });
		Gfx::DrawString(deltaTime.c_str(), { 0.0f, 90.0f });
		Gfx::DrawString(timeScale.c_str(), { 0.0f, 120.0f });

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
