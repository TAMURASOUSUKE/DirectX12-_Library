#include <TSLib.h>

// 生成されたSDKだけで初期化・描画・終了ができるか確認する
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// SDKの初期化に失敗した場合は異常終了する
	if (!TSLib::Initialize(L"TSGameLib SDK SmokeTest", 1280, 720))
	{
		return 1;
	}

	// テスト中の動きを目視できるように60FPSへ制限する
	Time::SetTargetFPS(60);

	constexpr int TEST_FRAME_COUNT{ 120 };
	int currentFrame{ 0 };

	while (TSLib::ProcessMessage() && currentFrame < TEST_FRAME_COUNT)
	{
		TSLib::BeginFrame();

		Gfx::ClearScreen(0.08f, 0.12f, 0.18f, 1.0f);

		// フレームが進んでいることを確認できるように矩形を移動させる
		const float positionX{ 100.0f + static_cast<float>(currentFrame) * 4.0f };

		Gfx::DrawBox({ positionX, 300.0f }, { positionX + 100.0f, 400.0f }, 0.0f, { 0.2f, 0.8f, 1.0f, 1.0f });

		// 内蔵フォントが外部リソースなしで使えることも確認する
		Gfx::DrawString("TSGameLib SDK SmokeTest", { 30.0f, 30.0f }, 1.0f);

		TSLib::EndFrame();

		++currentFrame;
	}

	TSLib::Finish();

	// 120フレーム完走した場合だけ成功として0を返す
	if (currentFrame != TEST_FRAME_COUNT)
	{
		return 2;
	}

	return 0;
}
