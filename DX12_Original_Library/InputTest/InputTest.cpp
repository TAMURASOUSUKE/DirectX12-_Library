#include <algorithm>
#include <array>
#include <cstdint>
#include <string> 
#include "../Src/Facade/TSLib.h"

// ボタンテスト用構造体
struct UIButtonEventStats
{
	std::uint64_t targetFrames{ 0 };
	std::uint64_t pushedCount{ 0 };
	std::uint64_t heldFrames{ 0 };
	std::uint64_t releasedCount{ 0 };
	std::uint64_t activatedCount{ 0 };
	std::uint64_t canceledCount{ 0 };
};

// エントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{

	// 初期化 失敗したら-1を返す
	if (!TSLib::Initialize(L"InputTest", 1280, 720)) return -1;
	// マウスカーソルの状態を設定
	bool isLocked{ false };
	if (!System::SetCursorMode(System::CursorMode::Normal)) DEBUG_LOG_ERROR("マウスカーソルの状態設定に失敗しました\n");

	enum class ActionMap { Jump, Dash, UISelect, Count }; // 抽象化テスト用アクション
	Input::SetupActions(ActionMap::Count); // 初期化
	Input::AddActionBinding(ActionMap::Jump, KeyCode::Button::SPACE);
	Input::AddActionBinding(ActionMap::Jump, PadCode::Button::A);
	Input::AddActionBinding(ActionMap::Jump, MouseCode::Click::LEFT);
	Input::AddActionBinding(ActionMap::Dash, KeyCode::Button::LSHIFT);
	Input::AddActionBinding(ActionMap::Dash, PadCode::Trigger::RIGHT);
	Input::AddActionBinding(ActionMap::Dash, MouseCode::Click::RIGHT);
	Input::AddActionBinding(ActionMap::UISelect, KeyCode::Button::RETURN);
	Input::AddActionBinding(ActionMap::UISelect, PadCode::Button::A);
	Input::AddActionBinding(ActionMap::UISelect, MouseCode::Click::LEFT);

	// Axis抽象化テスト用
	enum class AxisMap { Move, Look, Menu, Count };
	Input::SetupAxes(AxisMap::Count); // Axisの席数を初期化
	Input::SetAxisMode(AxisMap::Move, AxisMode::Value); // Moveは方向・傾きそのものを返すValue
	// WASDをMoveに登録(WASDはDgitalなのでDigitalAxisBinding)
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::W, Vector2{ 0.0f, -1.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::A, Vector2{ -1.0f, 0.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::S, Vector2{ 0.0f, 1.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Move, DigitalAxisBinding{ KeyCode::Button::D, Vector2{ 1.0f, 0.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	// Padの左スティック登録 (スティックなのでStickAxisBinding)
	Input::AddAxisBinding(AxisMap::Move, StickAxisBinding{ PadCode::Stick::LEFT, 1.0f, false}); //  StickAxisBinding = 指定スティック、大きさ、Y反転

	// 右スティックをカメラように登録
	Input::SetAxisMode(AxisMap::Look, AxisMode::Delta); // Lookは1フレームで動かす量を返すDeltaとする
	Input::AddAxisBinding(AxisMap::Look, MouseDeltaAxisBinding{ 0.05f, false }); // マウスの登録(マウスなのでMouseDeltaAxisBinding) MouseDeltaAxisBinding = 大きさ、Y反転
	Input::AddAxisBinding(AxisMap::Look, StickAxisBinding{ PadCode::Stick::RIGHT, 180.0f, false }); // 右スティックの登録

	// UI操作の登録
	Input::SetAxisMode(AxisMap::Menu, AxisMode::Value);
	Input::AddAxisBinding(AxisMap::Menu, DigitalAxisBinding{ KeyCode::Button::W, Vector2{ 0.0f, -1.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Menu, DigitalAxisBinding{ KeyCode::Button::A, Vector2{ -1.0f, 0.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Menu, DigitalAxisBinding{ KeyCode::Button::S, Vector2{ 0.0f, 1.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Menu, DigitalAxisBinding{ KeyCode::Button::D, Vector2{ 1.0f, 0.0f }, 1.0f }); //  DigitalAxisBinding = 指定キー、方向、大きさ
	Input::AddAxisBinding(AxisMap::Menu, StickAxisBinding{ PadCode::Stick::RIGHT, 1.0f, false }); // 右スティックの登録
	 
	int wheelValue{ 0 }; // マウスホイールを動かしたときに生値
	int testWheelNotch{ 0 }; // マウスホイールを動かした回数

	constexpr int BUTTON_COLUMNS{ 5 }; // ボタンの列数
	constexpr int BUTTON_COUNT{ 23 }; //　ボタンの個数

	// ボタンのサイズ定義
	constexpr float BUTTON_LEFT{ 650.0f };
	constexpr float BUTTON_TOP{ 100.0f };
	constexpr float BUTTON_WIDTH{ 105.0f };
	constexpr float BUTTON_HEIGHT{ 65.0f };
	constexpr float BUTTON_GAP{ 10.0f };

	std::array<UIButtonHandle, BUTTON_COUNT> buttons{};
	std::array<Rect, BUTTON_COUNT> buttonRects{};
	std::array<UIButtonEventStats, BUTTON_COUNT> eventStats{};
	int inspectedButtonIndex{ 0 };
	std::string lastEvent{ "None" };

	// ボタンのナビゲーションシステムの拡張と設定
	if (!UI::SetupNavigation(BUTTON_COLUMNS))
	{
		TSLib::Finish();
		return -1;
	}
	if (!UI::SetNavigationAxis(AxisMap::Menu))
	{
		TSLib::Finish();
		return -1;
	}

	// ボタンの配置
	for (int i = 0; i < BUTTON_COUNT; i++)
	{
		const int row{ i / BUTTON_COLUMNS }; // 行
		const int column{ i % BUTTON_COLUMNS }; // 列

		Vector2 position{ BUTTON_LEFT + column * (BUTTON_WIDTH + BUTTON_GAP), BUTTON_TOP + row * (BUTTON_HEIGHT + BUTTON_GAP) };
	
		// このテストでは全て同様の矩形あたり判定
		buttonRects.at(static_cast<std::size_t>(i)) = Rect{ position, {BUTTON_WIDTH, BUTTON_HEIGHT} };

		// 行に分けて操作テストを分けるのでCreateの各形式をまとめて検査
		if (i < 5) buttons[i] = UI::Create(KeyCode::Button::RETURN, buttonRects.at(static_cast<std::size_t>(i))); // キー
		else if (i < 10) buttons[i] = UI::Create(PadCode::Button::A, buttonRects.at(static_cast<std::size_t>(i))); // パッド
		else if (i < 15) buttons[i] = UI::Create(MouseCode::Click::LEFT, buttonRects.at(static_cast<std::size_t>(i))); // マウス
		else if (i < 20) buttons[i] = UI::Create(ActionMap::UISelect, buttonRects.at(static_cast<std::size_t>(i))); // 抽象化
		else buttons[i] = UI::Create(buttonRects[i]); // Enter,A,左クリックの標準版

		if (!buttons[i].IsValid())
		{
			DEBUG_LOG_ERROR("ボタンの生成に失敗しました index : {}\n", i);
			// 失敗したら全てのボタンを削除
			for (int created = 0; created < i; created++)
			{
				UI::DestroyButton(buttons[created]);
			}
			TSLib::Finish();
			return -1;
		}

		// 失敗しても指定状態にならないだけなのでログを出す
		if (!UI::AddNavigationButton(buttons.at(static_cast<std::size_t>(i)))) DEBUG_LOG_ERROR("UIButtonNavigation登録に失敗しました index : {} \n", i);
	
		UIButtonEventStats* const buttonStats{ &eventStats.at(static_cast<std::size_t>(i)) };

		bool eventRegistrationSucceeded{ true };
		// ターゲット時の動作設定
		eventRegistrationSucceeded &= UI::SetOnTarget
		(
			buttons.at(static_cast<std::size_t>(i)),
			[buttonStats, &inspectedButtonIndex, i]()
			{
				buttonStats->targetFrames++;
				inspectedButtonIndex = i;
			}
		);
		// プッシュ時の動作設定
		eventRegistrationSucceeded &= UI::SetOnPushed
		(
			buttons.at(static_cast<std::size_t>(i)),
			[buttonStats, &inspectedButtonIndex, &lastEvent, i]()
			{
				buttonStats->pushedCount++;
				inspectedButtonIndex = i;
				lastEvent = std::format("Button {} : Pushed", i + 1);
			}
		);
		// プレス時の動作設定
		eventRegistrationSucceeded &= UI::SetOnHeld
		(
			buttons.at(static_cast<std::size_t>(i)),
			[buttonStats, &inspectedButtonIndex, i]()
			{
				buttonStats->heldFrames++;
				inspectedButtonIndex = i;
			}
		);
		// リリース時の動作設定
		eventRegistrationSucceeded &= UI::SetOnReleased
		(
			buttons.at(static_cast<std::size_t>(i)),
			[buttonStats, &inspectedButtonIndex, &lastEvent, i]()
			{
				buttonStats->releasedCount++;
				inspectedButtonIndex = i;
				lastEvent = std::format("Button {} : Released", i + 1);
			}
		);
		// 完了時の動作設定
		eventRegistrationSucceeded &= UI::SetOnActivated
		(
			buttons.at(static_cast<std::size_t>(i)),
			[buttonStats, &inspectedButtonIndex, &lastEvent, i]()
			{
				buttonStats->activatedCount++;
				inspectedButtonIndex = i;
				lastEvent = std::format("Button {} : Activated", i + 1);
			}
		);
		// キャンセル時の操作設定
		eventRegistrationSucceeded &= UI::SetOnCanceled
		(
			buttons.at(static_cast<std::size_t>(i)),
			[buttonStats, &inspectedButtonIndex, &lastEvent, i]()
			{
				buttonStats->canceledCount++;
				inspectedButtonIndex = i;
				lastEvent = std::format("Button {} : Canceled", i + 1);
			}
		);

		if (!eventRegistrationSucceeded) DEBUG_LOG_ERROR("UIButtonのイベント登録に失敗しました index : {}", i);

	}

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
		const std::string moveAxisStr{ std::format("MoveAxis\nx : {:.3f}, y : {:.3f}, length : {:.3f}", moveAxis.x, moveAxis.y, moveAxis.Length()) };
		const std::string lookAxisStr{ std::format("LookAxis\nx : {:.3f}, y : {:.3f}", lookAxis.x, lookAxis.y) };
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

		const Vector4 normalColor{ 0.20f, 0.20f, 0.25f, 1.0f };
		const Vector4 hoveredColor{ 0.15f, 0.45f, 0.85f, 1.0f };
		const Vector4 pressedColor{ 0.85f, 0.45f, 0.15f, 1.0f };

		for (int i = 0; i < BUTTON_COUNT; i++)
		{
			// 描画色の決定
			Vector4 color{ normalColor };
			switch (UI::GetVisualState(buttons[i]))
			{
			case UIButtonVisualState::Hovered:
				color = hoveredColor;
				break;
			case UIButtonVisualState::Pressed:
				color = pressedColor;
				break;
			case UIButtonVisualState::Disabled:
				color = { 0.1f, 0.1f, 0.1f, 1.0f };
				break;
			case UIButtonVisualState::Normal:
			default:
				break;
			}

			// 描画サイズ
			const Vector2 min{ buttonRects.at(static_cast<std::size_t>(i)).GetMinPos()};
			const Vector2 max{ buttonRects.at(static_cast<std::size_t>(i)).GetMaxPos() };

			// 各行によって文字列を変更 
			const char* inputName{ "DEFAULT" };
			if (i < 5) inputName = "KEY";
			else if (i < 10) inputName = "PAD";
			else if (i < 15) inputName = "MOUSE";
			else if (i < 20) inputName = "ACTION";

			// iはループ条件によって0～22が保証されている
			const UIButtonEventStats& buttonEventStats{ eventStats.at(static_cast<std::size_t>(i)) };
			const std::string text{ std::format("{} {} A:{}", inputName, i + 1, buttonEventStats.activatedCount) };
			Gfx::DrawBox(min, max, 0.0f, color, true);
			Gfx::DrawString(text.c_str(), min + Vector2{ 5.0f, 20.0f }, 0.45f, Vector4::One);
		}
	
		const int safeInspectedIndex{ std::clamp(inspectedButtonIndex, 0, BUTTON_COUNT - 1) };
		if (safeInspectedIndex != inspectedButtonIndex)
		{
			DEBUG_LOG_ERROR("イベント表示対象のButtonIndexが範囲外です Index : {} Count : {}\n", inspectedButtonIndex, BUTTON_COUNT);
			inspectedButtonIndex = safeInspectedIndex;
		}

		const UIButtonEventStats& stats{ eventStats.at(static_cast<std::size_t>(safeInspectedIndex)) };
		const std::string inspectedText{ std::format("Inspect Button : {}", inspectedButtonIndex + 1) };
		const std::string targetText{ std::format("Target Frames : {}", stats.targetFrames) };
		const std::string pushedText{ std::format("Pushed : {}", stats.pushedCount) };
		const std::string heldText{ std::format("Held Frames : {}", stats.heldFrames) };
		const std::string releasedText{ std::format("Released : {}", stats.releasedCount) };
		const std::string activatedText{ std::format("Activated : {}", stats.activatedCount) };
		const std::string canceledText{ std::format("Canceled : {}", stats.canceledCount) };

		Gfx::DrawString(inspectedText.c_str(), { 0.0f, 510.0f }, 0.7f);
		Gfx::DrawString(targetText.c_str(), { 0.0f, 535.0f }, 0.7f);
		Gfx::DrawString(pushedText.c_str(), { 0.0f, 560.0f }, 0.7f);
		Gfx::DrawString(heldText.c_str(), { 0.0f, 585.0f }, 0.7f);
		Gfx::DrawString(releasedText.c_str(), { 0.0f, 610.0f }, 0.7f);
		Gfx::DrawString(activatedText.c_str(), { 0.0f, 635.0f }, 0.7f);
		Gfx::DrawString(canceledText.c_str(), { 0.0f, 660.0f }, 0.7f);
		Gfx::DrawString(lastEvent.c_str(), { 0.0f, 685.0f }, 0.65f);

		TSLib::EndFrame(); // フレーム終了処理
	}

	for (UIButtonHandle button : buttons)
	{
		UI::DestroyButton(button);
	}

	TSLib::Finish(); // 終了
	return 0;
}
