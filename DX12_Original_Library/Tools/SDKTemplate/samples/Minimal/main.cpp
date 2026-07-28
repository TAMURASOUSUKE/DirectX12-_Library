#include <TSLib.h>

// SDKだけで起動・描画・入力・時間管理が使えることを確認する最小サンプル
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// SDKの初期化に失敗した場合は終了する
	if (!TSLib::Initialize(L"TSGameLib SDK Test", 1280, 720))
	{
		return -1;
	}

	Vector2 boxPosition{ 200.0f, 300.0f };
	const Vector2 boxSize{ 120.0f, 120.0f };
	const float moveSpeed{ 300.0f };

	Time::SetTargetFPS(60);

	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame();

		// DeltaTimeを使って、FPSに依存しない速度で移動する
		if (Input::IsKeyPress(KeyCode::Button::LEFT))
		{
			boxPosition.x -= moveSpeed * Time::DeltaTime();
		}

		if (Input::IsKeyPress(KeyCode::Button::RIGHT))
		{
			boxPosition.x += moveSpeed * Time::DeltaTime();
		}

		Gfx::ClearScreen(0.08f, 0.12f, 0.18f, 1.0f);

		// 基礎図形描画の確認
		Gfx::DrawBox(boxPosition, boxPosition + boxSize, 0.0f, { 0.2f, 0.8f, 1.0f, 1.0f });

		Gfx::DrawCircle({ 640.0f, 360.0f }, 80.0f, { 1.0f, 0.3f, 0.4f, 1.0f });

		// 内蔵フォントが外部ファイルなしで表示できることも確認する
		Gfx::DrawString("TSGameLib SDK OK", { 30.0f, 30.0f }, 1.0f);

		TSLib::EndFrame();
	}

	TSLib::Finish();
	return 0;
}
