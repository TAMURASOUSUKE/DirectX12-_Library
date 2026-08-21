#include <string> 
#include "../Src/Facade/TSLib.h"


// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"InputTest", 1280, 720)) return -1;
	// マウスカーソルの状態を設定
	bool isLocked{ true }; // Initialize直後にLockedにした状態と合わせる
	if (!System::SetCursorMode(System::CursorMode::Locked)) DEBUG_LOG_ERROR("マウスカーソルの状態設定に失敗しました\n");

	enum class ActionMap { Jump, Dash, Count }; // 抽象化テスト用アクション
	Input::SetupActions(ActionMap::Count); // 初期化
	Input::AddActionBinding(ActionMap::Jump, KeyCode::Button::SPACE);
	Input::AddActionBinding(ActionMap::Jump, PadCode::Button::A);
	Input::AddActionBinding(ActionMap::Jump, MouseCode::Click::LEFT);
	Input::AddActionBinding(ActionMap::Dash, KeyCode::Button::LSHIFT);
	Input::AddActionBinding(ActionMap::Dash, PadCode::Trigger::RIGHT);
	Input::AddActionBinding(ActionMap::Dash, MouseCode::Click::RIGHT);

	// Axis抽象化テスト用
	enum class AxisMap { Move, Look, Count };
	Input::SetupAxes(AxisMap::Count); // Axisの席数を初期化
	Input::SetAxisMode(AxisMap::Move, AxisMode::Value); // Moveは方向・傾きそのものを返すValue
	// WASDをMoveに登録(WASDはDgitalなのでDigitalAxisBinding)
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::W, Vector2{ 0.0f, -1.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::A, Vector2{ -1.0f, 0.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::S, Vector2{ 0.0f, 1.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::D, Vector2{ 1.0f, 0.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	// Padの左スティック登録 (スティックなのでStickAxisBinding)
	Input::AddAxisBinding(AxisMap::Move, StickAxisBinding{ PadCode::Stick::LEFT, 1.0f, false}); //  StickAxisBinding = 指定スティック、大きさ、Y反転
	Input::SetAxisMode(AxisMap::Look, AxisMode::Delta); // Lookは1フレームで動かす量を返すDeltaとする
	Input::AddAxisBinding(AxisMap::Look, MouseDeltaAxisBinding{ 0.05f, false }); // マウスの登録(マウスなのでMouseDeltaAxisBinding) MouseDeltaAxisBinding = 大きさ、Y反転
	Input::AddAxisBinding(AxisMap::Look, StickAxisBinding{ PadCode::Stick::RIGHT, 180.0f, false }); // 右スティックの登録

	 
	int wheelValue{ 0 }; // マウスホイールを動かしたときに生値
	int testWheelNotch{ 0 }; // マウスホイールを動かした回数

	while (TSLib::ProcessMessage() && !Input::IsKeyPushed(KeyCode::Button::ESC))
	{
		TSLib::BeginFrame(); // フレーム開始処理
		// カーソル状態設定
		if (Input::IsKeyPushed(KeyCode::Button::TAB))
		{
			isLocked = !isLocked;
			const System::CursorMode mode{ isLocked ? System::CursorMode::Locked : System::CursorMode::Normal };
			if (!System::SetCursorMode(mode)) DEBUG_LOG_ERROR("マウスカーソルの状態変更に失敗しました\n");
		}

		// 値取得
		const Vector2Int cursorPos{ Input::GetMousePoint() }; // マウス座標
		const Vector2Int cursorDelta{ Input::GetMouseDelta() }; // マウス移動量
		const Vector2 stickValue{ Input::GetPadStickValue(PadCode::Stick::LEFT) }; // 左スティックの値
		const Vector2 moveAxis{ Input::GetAxisValue(AxisMap::Move) };  // Axis抽象化のMove値
		const Vector2 lookAxis{ Input::GetAxisValue(AxisMap::Look) }; // Axis抽象化のLook値
		const InputMethod currentInputMethod{ Input::GetInputMethod() }; // 現在の操作方式
		const float inputTrigger{ Input::GetPadTriggerValue(PadCode::Trigger::LEFT) }; // 左トリガーの値
		wheelValue += Input::GetMouseWheelValue(); // マウスの回転量
		testWheelNotch += Input::GetMouseWheelNotchValue(); // マウスの回転回数

		// 各文字列化
		const std::string cursorStr{ std::format("Input CursorPosition x : {},  y : {}", cursorPos.x, cursorPos.y) };
		const std::string cursorDeltaStr{ std::format("Input CursorDelta x : {},  y : {}", cursorDelta.x, cursorDelta.y) };
		const std::string stickValueStr{ std::format("Input LeftStickValue x : {:.2f}, y : {:.2f}", stickValue.x, stickValue.y) };
		const std::string triggerStr{ std::format("Input Trigger Value : {:.2f}", inputTrigger) };
		const std::string wheelValueStr{ std::format("Input Wheel Value : {}", wheelValue) };
		const std::string wheelNotchValueStr{ std::format("Input WheelNotch Value : {}", testWheelNotch) };
		// キーボード
		const std::string AKeyStr{ Input::IsKeyPress(KeyCode::Button::A) ? "AKeyPress\n" : "AKeyNoPres\n" };
		const std::string ReturnKeyStr{ Input::IsKeyPress(KeyCode::Button::RETURN) ? "ReturnKeyPress\n" : "ReturnKeyNoPress\n" };
		const std::string D4KeyStr{ Input::IsKeyPress(KeyCode::Button::D4) ? "Number4KeyPress\n" : "Number4KeyNoPress\n" };
		// パッド
		const std::string AButtonStr{ Input::IsPadPress(PadCode::Button::A) ? "AButtonPress\n" : "AButtonNoPress\n" };
		const std::string XButtonStr{ Input::IsPadPress(PadCode::Button::X) ? "XButtonPress\n" : "XButtonNoPress\n" };
		const std::string StartButtonStr{ Input::IsPadPress(PadCode::Button::START) ? "StartButtonPress\n" : "StartButtonNoPress\n" };
		// 抽象化
		const std::string DashActionStr{ Input::IsActionPress(ActionMap::Dash) ? "DashActionPress\n" : "DashActionNoPush"};
		const std::string JumpActionStr{ Input::IsActionPress(ActionMap::Jump) ? "JumpActionPress\n" : "JumpActionNoPress" };
		const std::string moveAxisStr{ std::format("MoveAxis x : {:.3f}, y : {:.3f}, length : {:.3f}", moveAxis.x, moveAxis.y, moveAxis.Length()) };
		const std::string lookAxisStr{ std::format("LookAxis x : {:.3f}, y : {:.3f}", lookAxis.x, lookAxis.y) };
		const std::string inputMethodStr{ currentInputMethod == InputMethod::KeyboardMouse ? "InputMethod : KeyboardMouse" : "InputMethod : GamePad" };

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
		Gfx::DrawString(moveAxisStr.c_str(), { 0.0f, 420.0f });
		Gfx::DrawString(lookAxisStr.c_str(), { 0.0f, 450.0f });
		Gfx::DrawString(inputMethodStr.c_str(), { 0.0f, 480.0f });

		TSLib::EndFrame(); // フレーム終了処理
	}

	TSLib::Finish(); // 終了
	return 0;
}
