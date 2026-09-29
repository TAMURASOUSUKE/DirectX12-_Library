#include <windows.h>
#include <cstddef>
#include <string_view>
#include <array>
#include "../Debug/DebugLogs.h"
#include "../Graphics/GraphicsMetrics.h"
#include "GfxInternal.h"
#include "Gfx.h" // デバッグシステムとつなぐために必要
#include "InputInternal.h"
#include "SoundInternal.h"
#include "TimeInternal.h"
#include "SystemInternal.h"
#include "UIInternal.h"
#include "DebugInternal.h"
#include "TSLib.h"

namespace 
{
	constexpr std::size_t PASS_COUNT{ static_cast<std::size_t>(GraphicsPass::Count) }; // パスの数
	DebugChannelID renderingChannelID{};
	DebugMetricID drawCallMetricID{};

	// パスごとの表示名(個数を推論させることでstatic_assertに引っかかるようにする)
	constexpr auto passName = std::array
	{
		std::string_view{ "Background Sprite" },
		std::string_view{ "Shadow" },
		std::string_view{ "Opaque Model" },
		std::string_view{ "3D Primitive" },
		std::string_view{ "Blend Model" },
		std::string_view{ "Foreground Sprite" },
		std::string_view{ "2D Shape" },
		std::string_view{ "Debug Preview" },
		std::string_view{ "PostEffect" },
		std::string_view{ "Terrain" },
	};
	std::array<DebugMetricID, PASS_COUNT> passDrawCallID{};

	static_assert(passName.size() == PASS_COUNT, "表示名がパスの数とあっていません");
}

// 初期化
bool TSLib::Initialize(const wchar_t* _title, int _width, int _height)
{
	return Initialize(_title, _width, _height, System::WindowMode::Windowed);
}

bool TSLib::Initialize(const wchar_t* _title, int _virtualWidth, int _virtualHeight, System::WindowMode _mode)
{
	HRESULT comResult{};
	comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED); // COMを初期化
	DEBUG_ASSERT(SUCCEEDED(comResult));
	if (FAILED(comResult))
	{
		return false;
	}

	bool result{ false };

	TimeInternal::Initialize();
	result = SystemInternal::Initialize(_title, _virtualWidth, _virtualHeight, _mode);
	DEBUG_ASSERT(result && "Windowの初期化に失敗しました\n");
	if (!result) return result;
	const Vector2Int clientSize{ SystemInternal::GetClientSize() };
	result = GfxInternal::Initialize(SystemInternal::GetHWND(), clientSize.x, clientSize.y, _virtualWidth, _virtualHeight); // グラフィックの初期化とウィンドウ作成
	DEBUG_ASSERT(result && "ゲームの初期化に失敗しました\n");
	if (!result) return result;
	result = DebugInternal::Initialize(); // デバッグ表示の初期化
	DEBUG_ASSERT(result && "デバッグ機能の初期化に失敗しました\n");
	if (!result) return result;

	// ライブラリのデフォルトで出すデバッグパラメータ
	renderingChannelID = Debug::RegisterChannel("Rendering"); // 描画関連のチャンネル登録
	drawCallMetricID = Debug::RegisterMetric({ "Draw Calls", renderingChannelID, DebugMetricUnit::Count, DebugMetricAggregation::Set }); // Graphics側で合計しているのでSet
	// デバッグ機能の失敗でゲームを機能不能にはしない
	if (!renderingChannelID.IsValid() || !drawCallMetricID.IsValid()) DEBUG_LOG_ERROR("内蔵Rendering Metricの登録に失敗しました\n");

	// 各パス別Metricを登録
	if (renderingChannelID.IsValid())
	{
		for (std::size_t i = 0; i < PASS_COUNT; i++)
		{
			// 明示的なstring変換を行う(コピーコストが発生するがInitializeの一回だけなので許容する)
			passDrawCallID[i] = Debug::RegisterMetric({std::string{passName[i]}, renderingChannelID, DebugMetricUnit::Count, DebugMetricAggregation::Set});
			if (!passDrawCallID[i].IsValid()) DEBUG_LOG_ERROR("%d番目のパス登録に失敗しました\n", i);
		}
	}

	result = InputInternal::Initialize(SystemInternal::GetHWND());
	DEBUG_ASSERT(result && "入力処理の初期化に失敗しました\n");
	if (!result) return result;
	UIInternal::Initialize(); // UIはvoidなのでチェックなし
	result = SoundInternal::Initialize(); // XAudio2はCoInitializeに依存するため初期化が行われるGfxの後に初期化
	DEBUG_ASSERT(result && "音処理の初期化に失敗しました\n");
	if (!result) return result;

	// コールバックの配線接続 : ラムダで渡す
	SystemInternal::SetOnWheel([](short _delta) { InputInternal::AddMouseWheelDelta(_delta); });
	SystemInternal::SetOnResize([](int _width, int _height) { GfxInternal::RequestResize(_width, _height); });
	SystemInternal::SetOnCursorWarp([]() { InputInternal::ResetMouseCursorTracking(); });
	return result;
}

// メッセージループ
bool TSLib::ProcessMessage()
{
	MSG msg{}; // イベント情報を格納する型
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		if (msg.message == WM_QUIT) return false;
		DispatchMessage(&msg);
	}
	return true;
}

void TSLib::BeginFrame()
{
	DebugInternal::BeginFrame(); // デバッグ表示用のフレームの開始処理
	if (drawCallMetricID.IsValid())
	{
		const GraphicsFrameMetrics& graphicsMetrics{ GfxInternal::GetLastFrameMetrics() }; // 前フレームの値を検出
		Debug::SubmitMetric(drawCallMetricID, static_cast<double>(graphicsMetrics.drawCallCount)); // 全体個数書き込み
		
		for (std::size_t i = 0; i < PASS_COUNT; i++)
		{
			if (!passDrawCallID[i].IsValid()) continue;
			Debug::SubmitMetric(passDrawCallID[i], static_cast<double>(graphicsMetrics.passDrawCallCount[i])); // パス個数書き込み
		}

	}
	TimeInternal::BeginFrame(); // 時間関連のフレーム最初の処理
	InputInternal::BeginFrame(Time::UnscaledDeltaTime()); // 入力の最初の処理
	UIInternal::BeginFrame({Time::UnscaledDeltaTime(), System::GetClientSize(), Gfx::GetVirtualSize()}); // 入力の更新後にUIの更新
	SystemInternal::UpdateCursorLock(); // マウス移動量を取得した後に中央へ
	GfxInternal::BeginFrame(); // グラフィックのフレーム最初の処理
	SoundInternal::BeginFrame(Time::UnscaledDeltaTime()); // 音関連のフレーム最初の処理
}

void TSLib::EndFrame()
{
	SoundInternal::EndFrame(); // 音関連のフレーム最後の処理
	DebugInternal::EndFrame(); // デバッグ表示用のフレームの最後の処理
	const DebugOverlayFrame& debugOverlayFrame{ DebugInternal::GetOverlayFrame() }; // 描画とデバッグを非依存にするためにTSLib側からつなげる
	for (const DebugTextCommand& command : debugOverlayFrame.textCommands)
	{
		// ここでデバッグ類の描画
		Gfx::DrawString(command.text.c_str(), command.position, command.scale, command.color);
	}
	GfxInternal::EndFrame(); // グラフィックのフレーム最後の処理
	InputInternal::EndFrame(); // 入力関連のフレーム最後の処理
	TimeInternal::EndFrame(); // 時間関連のフレーム最後の処理
}

void TSLib::Finish()
{
	SoundInternal::Finish(); // 音の終了処理
	UIInternal::Finish(); // 入力更新より前に終了処理
	InputInternal::Finish(); // 入力の終了処理
	DebugInternal::Finish(); // デバッグ機能関連の終了
	GfxInternal::Finish(); // グラフィックの終了処理
	SystemInternal::Finish(); // システムの終了処理
	TimeInternal::Finish(); // 時間管理の終了処理
	CoUninitialize(); // COMも閉じる
}
