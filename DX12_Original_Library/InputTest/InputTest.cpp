#include <string> 
#include "../Src/Facade/TSLib.h"


/*
	パッケージ化前なので、DirectXTexの参照も入れていますが使っていません。
*/
// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"InputTest", 1280, 720)) return -1;


	enum class ActionMap { Jump, Dash, Count }; // 抽象化テスト用アクション
	Input::SetupActions(ActionMap::Count); // 初期化
	Input::SetAction(ActionMap::Jump, KeyCode::Button::SPACE);
	Input::SetAction(ActionMap::Jump, PadCode::Button::A);
	Input::SetAction(ActionMap::Jump, MouseCode::Click::LEFT);
	Input::SetAction(ActionMap::Dash, KeyCode::Button::LSHIFT);
	Input::SetAction(ActionMap::Dash, PadCode::Trigger::RIGHT);
	Input::SetAction(ActionMap::Dash, MouseCode::Click::RIGHT);

	int wheelValue{ 0 }; // マウスホイールを動かしたときに生値
	int testWheelNotch{ 0 }; // マウスホイールを動かした回数

	while (TSLib::ProcessMessage())
	{
		TSLib::BeginFrame(); // フレーム開始処理

		// 値取得
		Vector2Int cursorPos{ Input::GetMousePoint() }; // マウス座標
		Vector2Int cursorDelta{ Input::GetMouseDelta() }; // マウス移動量
		Vector2 stickValue{ Input::GetPadStickValue(PadCode::Stick::LEFT) }; // 左スティックの値
		float inputTrigger{ Input::GetPadTriggerValue(PadCode::Trigger::RIGHT) }; // 右トリガーの値
		wheelValue += Input::GetMouseWheelValue(); // マウスの回転量
		testWheelNotch += Input::GetMouseWheelNotchValue(); // マウスの回転回数

		// 各文字列化
		std::string cursorStr{ std::format("Input CursorPosition x : {},  y : {}", cursorPos.x, cursorPos.y) };
		std::string cursorDeltaStr{ std::format("Input CursorDelta x : {},  y : {}", cursorDelta.x, cursorDelta.y) };
		std::string stickValueStr{ std::format("Input LeftStickValue x : {:.2f}, y : {:.2f}", stickValue.x, stickValue.y) };
		std::string triggerStr{ std::format("Input Trigger Value : {:.2f}", inputTrigger) };
		std::string wheelValueStr{ std::format("Input Wheel Value : {}", wheelValue) };
		std::string wheelNotchValueStr{ std::format("Input WheelNotch Value : {}", testWheelNotch) };
		// キーボード
		std::string AKeyStr{ Input::IsKeyPress(KeyCode::Button::A) ? "AKeyPress\n" : "AKeyNoPres\n" };
		std::string ReturnKeyStr{ Input::IsKeyPress(KeyCode::Button::RETURN) ? "ReturnKeyPress\n" : "ReturnKeyNoPress\n" };
		std::string D4KeyStr{ Input::IsKeyPress(KeyCode::Button::D4) ? "Number4KeyPress\n" : "Number4KeyNoPress\n" };
		// パッド
		std::string AButtonStr{ Input::IsPadPress(PadCode::Button::A) ? "AButtonPress\n" : "AButtonNoPress\n" };
		std::string XButtonStr{ Input::IsPadPress(PadCode::Button::X) ? "XButtonPress\n" : "XButtonNoPress\n" };
		std::string StartButtonStr{ Input::IsPadPress(PadCode::Button::START) ? "StartButtonPress\n" : "StartButtonNoPress\n" };
		// 抽象化
		std::string DashActionStr{ Input::IsActionPress(ActionMap::Dash) ? "DashActionPress\n" : "DashActionNoPush"};
		std::string JumpActionStr{ Input::IsActionPress(ActionMap::Jump) ? "JumpActionPress\n" : "JumpActionNoPress" };

		Gfx::ClearScreen(); // 画面クリア(黒)

		Gfx::DrawString(cursorStr.c_str(), { 0.0f, 0.0f });
		Gfx::DrawString(cursorDeltaStr.c_str(), { 0.0f, 30.0f });
		Gfx::DrawString(triggerStr.c_str(), { 0.0f, 60.0f });
		Gfx::DrawString(stickValueStr.c_str(), { 0.0f, 90.0f });
		Gfx::DrawString(wheelValueStr.c_str(), { 0.0f, 120.0f });
		Gfx::DrawString(wheelNotchValueStr.c_str(), { 0.0f, 150.0f });
		Gfx::DrawString(AKeyStr.c_str(), { 0.0f, 180.0f });
		Gfx::DrawString(ReturnKeyStr.c_str(), { 0.0f, 210.0f });
		Gfx::DrawString(D4KeyStr.c_str(), { 0.0f, 240.0f });
		Gfx::DrawString(AButtonStr.c_str(), { 0.0f, 270.0f });
		Gfx::DrawString(XButtonStr.c_str(), { 0.0f, 300.0f });
		Gfx::DrawString(StartButtonStr.c_str(), { 0.0f, 330.0f });
		Gfx::DrawString(DashActionStr.c_str(), { 0.0f, 360.0f });
		Gfx::DrawString(JumpActionStr.c_str(), { 0.0f, 390.0f });

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
