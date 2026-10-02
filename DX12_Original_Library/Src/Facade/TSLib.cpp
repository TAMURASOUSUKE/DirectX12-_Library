#include <windows.h>
#include <cstddef>
#include <string_view>
#include <array>
#include "../Debug/DebugLogs.h"
#include "../Graphics/GraphicsMetrics.h"
#include "../Debug/CPUFrameTimingSystem.h"
#include "GfxInternal.h"
#include "Gfx.h" // デバッグシステムとつなぐために必要
#include "InputInternal.h"
#include "SoundInternal.h"
#include "TimeInternal.h"
#include "SystemInternal.h"
#include <array>
#include "UIInternal.h"
#include "DebugInternal.h"
#include "TSLib.h"

namespace {
	constexpr std::size_t PASS_COUNT{ static_cast<std::size_t>(GraphicsPass::Count) }; // パスの数

	CPUFrameTimingSystem cpuFrameTimingSystem{};
	DebugChannelID frameChannelID{};
	DebugChannelID renderingChannelID{};
	DebugChannelID inputChannelID{};
	DebugChannelID uiChannelID{};
	DebugChannelID soundChannelID{};
	DebugChannelID debugChannelID{};
	std::array<DebugMetricID, PASS_COUNT> passGPUTimeMetricID{}; 
	DebugMetricID drawCallMetricID{};
	DebugMetricID gpuFrameTimeMetricID{};
	DebugMetricID cpuFrameTimeMetricID{};
	DebugMetricID cpuBudgetUsageMetricID{}; 
	DebugMetricID gpuBudgetUsageMetricID{};
	DebugMetricID graphicsBeginCpuMetricID{};
	DebugMetricID inputUpdateCpuMetricID{};
	DebugMetricID cursorUpdateCpuMetricID{};
	DebugMetricID uiUpdateCpuMetricID{};
	DebugMetricID soundUpdateCpuMetricID{};
	DebugMetricID debugBeginCpuMetricID{};
	DebugMetricID debugMetricSubmitCpuMetricID{};

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

namespace 
{
	// パスごとのMetricを登録する
	void RegisterDefaultDebugMetrics()
	{
		// ライブラリのデフォルトで出すデバッグパラメータ
		frameChannelID = Debug::RegisterChannel("Frame"); // フレーム関連のチャンネル登録
		renderingChannelID = Debug::RegisterChannel("Rendering"); // 描画関連のチャンネル登録
		inputChannelID = Debug::RegisterChannel("Input"); // 入力関連のチャンネル登録
		uiChannelID = Debug::RegisterChannel("UI"); // UI関連のチャンネル登録
		soundChannelID = Debug::RegisterChannel("Sound"); // 音関連のチャンネル登録
		debugChannelID = Debug::RegisterChannel("Debug"); // デバッグ機能自身のチャンネル登録
		drawCallMetricID = Debug::RegisterMetric({ "Draw Calls", renderingChannelID, DebugMetricUnit::Count, DebugMetricAggregation::Set }); // Graphics側で合計しているのでSet
		cpuFrameTimeMetricID = Debug::RegisterMetric({ "CPU Frame Time", frameChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		cpuBudgetUsageMetricID = Debug::RegisterMetric({ "CPU Budget Usage", frameChannelID, DebugMetricUnit::Percent, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		gpuBudgetUsageMetricID = Debug::RegisterMetric({ "GPU Budget Usage", frameChannelID, DebugMetricUnit::Percent, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		graphicsBeginCpuMetricID = Debug::RegisterMetric({ "Graphics Begin CPU", renderingChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		inputUpdateCpuMetricID = Debug::RegisterMetric({ "Input Update CPU", inputChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		cursorUpdateCpuMetricID = Debug::RegisterMetric({ "Cursor Update CPU", inputChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		uiUpdateCpuMetricID = Debug::RegisterMetric({ "UI Update CPU", uiChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		soundUpdateCpuMetricID = Debug::RegisterMetric({ "Sound Update CPU", soundChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		debugBeginCpuMetricID = Debug::RegisterMetric({ "Debug Begin CPU", debugChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		// CPU・GPU結果の提出箇所が二つに分かれるためAddで合計する
		debugMetricSubmitCpuMetricID = Debug::RegisterMetric({ "Metric Submit CPU", debugChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Add, DebugMetricDisplayMode::WindowStatistics });
		// デバッグ機能の失敗でゲームを機能不能にはしない
		if (!renderingChannelID.IsValid() || !drawCallMetricID.IsValid() || !graphicsBeginCpuMetricID.IsValid()) DEBUG_LOG_ERROR("内蔵Rendering Metricの登録に失敗しました\n");
		if (!cpuFrameTimeMetricID.IsValid() || !cpuBudgetUsageMetricID.IsValid() || !gpuBudgetUsageMetricID.IsValid()) DEBUG_LOG_ERROR("内蔵Frame Metricの登録に失敗しました\n");
		if (!inputChannelID.IsValid() || !inputUpdateCpuMetricID.IsValid() || !cursorUpdateCpuMetricID.IsValid()) DEBUG_LOG_ERROR("内蔵Input Metricの登録に失敗しました\n");
		if (!uiChannelID.IsValid() || !uiUpdateCpuMetricID.IsValid()) DEBUG_LOG_ERROR("内蔵UI Metricの登録に失敗しました\n");
		if (!soundChannelID.IsValid() || !soundUpdateCpuMetricID.IsValid()) DEBUG_LOG_ERROR("内蔵Sound Metricの登録に失敗しました\n");
		if (!debugChannelID.IsValid() || !debugBeginCpuMetricID.IsValid() || !debugMetricSubmitCpuMetricID.IsValid()) DEBUG_LOG_ERROR("内蔵Debug Metricの登録に失敗しました\n");

		// 各パス別Metricを登録
		if (!renderingChannelID.IsValid()) return;
		for (std::size_t i = 0; i < PASS_COUNT; i++)
		{
			// 明示的なstring変換を行う(コピーコストが発生するがInitializeの一回だけなので許容する)
			passDrawCallID[i] = Debug::RegisterMetric({ std::string{passName[i]}, renderingChannelID, DebugMetricUnit::Count, DebugMetricAggregation::Set });
			if (!passDrawCallID[i].IsValid()) DEBUG_LOG_ERROR("%d番目のCPUDrawCallパス登録に失敗しました\n", i);

			std::string gpuMetricName{ "GPU " };
			gpuMetricName += std::string{ passName[i] };
			// Segment側で個別に合計を取っているのでSet
			passGPUTimeMetricID[i] = Debug::RegisterMetric({ gpuMetricName, renderingChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
			if (!passGPUTimeMetricID[i].IsValid()) DEBUG_LOG_ERROR("%d番目のGPU Pass Metric登録に失敗しました\n", i);
		}

		gpuFrameTimeMetricID = Debug::RegisterMetric({ "GPU Frame Time", frameChannelID, DebugMetricUnit::Milliseconds, DebugMetricAggregation::Set, DebugMetricDisplayMode::WindowStatistics });
		if (!gpuFrameTimeMetricID.IsValid()) DEBUG_LOG_ERROR("GPUフレーム時間Metricの登録に失敗しました\n");
	}

	// 各パスでの書き込みを行う
	void SubmitPassDrawCount()
	{
		if (!drawCallMetricID.IsValid()) return;

		const GraphicsFrameMetrics& graphicsMetrics{ GfxInternal::GetLastFrameMetrics() }; // 前フレームの値を検出
		Debug::SubmitMetric(drawCallMetricID, static_cast<double>(graphicsMetrics.drawCallCount)); // 全体個数書き込み

		for (std::size_t i = 0; i < PASS_COUNT; i++)
		{
			if (!passDrawCallID[i].IsValid()) continue;
			Debug::SubmitMetric(passDrawCallID[i], static_cast<double>(graphicsMetrics.passDrawCallCount[i])); // パス個数書き込み
		}
	}

	// デバッグ機能の描画
	void DebugDraw()
	{
		const DebugOverlayFrame& debugOverlayFrame{ DebugInternal::GetOverlayFrame() }; // 描画とデバッグを非依存にするためにTSLib側からつなげる
		const DebugFrameData& debugFrame{ DebugInternal::GetFrameData() }; // デバッグ形状データを読み取る
		for (const DebugTextCommand& command : debugOverlayFrame.textCommands)
		{
			// ここでデバッグ類の描画
			Gfx::DrawString(command.text.c_str(), command.position, command.scale, command.color);
		}

		// 各デバッグ形状の描画
		// 線分
		for (const DebugLineCommand& command : debugFrame.lineCommands)
		{
			Gfx::DrawLine3D(command.start, command.end, command.color);
		}

		// 箱
		for (const DebugBoxCommand& command : debugFrame.boxCommands)
		{
			const Box box{ command.center, command.halfSize, command.rotation };
			Gfx::DrawBox3D(box, command.color);
		}

		// 球
		for (const DebugSphereCommand& command : debugFrame.sphereCommands)
		{
			const float diameter{ command.radius * 2.0f };

			Transform transform{};
			transform.SetPosition(command.center);
			transform.SetScale({ diameter, diameter, diameter });

			Gfx::DrawSphere3D(transform, command.color, Gfx::Primitive3DStyle::DebugLine);
		}

		// capsule
		for (const DebugCapsuleCommand& command : debugFrame.capsuleCommands)
		{
			Gfx::DrawCapsule3D(command.start, command.end, command.radius, command.color, Gfx::Primitive3DStyle::DebugLine);
		}
	}

	// CPUGPU共通で使用率を計算する関数
	bool TryCalculateFrameBudgetUsage(double _elapsedMilliseconds, int _targetFPS, double& _outUsage)
	{
		_outUsage = 0.0;

		// FPS制限なしでは基準時間が決められない
		if (_targetFPS == 0) return false;

		if (_targetFPS < 0)
		{
			DEBUG_LOG_ERROR("不正なFPSが設定されています\n");
			return false;
		}

		if (_elapsedMilliseconds < 0)
		{
			DEBUG_LOG_ERROR("経過時間が負の値になっています\n");
			return false;
		}

		const double frameBudgetMilliseconds{ 1000.0 / static_cast<double>(_targetFPS) };
		const double usageRate{ (_elapsedMilliseconds / frameBudgetMilliseconds) * 100.0 }; // 使用率
		_outUsage = usageRate;
		return true;
	}

	// 前回完了したCPUフレーム時間をDebug Metricへ提出する
	void SubmitCPUFrameTiming()
	{
		const CPUFrameTimingFrame& cpuFrame{ cpuFrameTimingSystem.GetLastFrame() };
		if (!cpuFrameTimingSystem.GetLastFrame().valid) return;

		if (cpuFrameTimeMetricID.IsValid()) Debug::SubmitMetric(cpuFrameTimeMetricID, cpuFrame.milliseconds);

		double budgetUsage{ 0.0 };

		// 使用率の提出
		if (cpuBudgetUsageMetricID.IsValid() && TryCalculateFrameBudgetUsage(cpuFrame.milliseconds, cpuFrame.targetFPS, budgetUsage))
		{
			Debug::SubmitMetric(cpuBudgetUsageMetricID, budgetUsage);
		}
	}

	// GPUフレーム全体の時間を書き込む
	void SubmitGPUFrameTime()
	{

		const GraphicsGPUTimingFrame& gpuTimingFrame{ GfxInternal::GetLastGPUTimingFrame() };

		if (gpuTimingFrame.total.valid)
		{
			if (gpuFrameTimeMetricID.IsValid()) Debug::SubmitMetric(gpuFrameTimeMetricID, gpuTimingFrame.total.milliseconds);

			double budgetUsage{ 0.0 };

			if (gpuBudgetUsageMetricID.IsValid() && TryCalculateFrameBudgetUsage(gpuTimingFrame.total.milliseconds, Time::GetTargetFPS(), budgetUsage))
			{
				Debug::SubmitMetric(gpuBudgetUsageMetricID, budgetUsage);
			}
		}

		for (std::size_t i = 0; i < PASS_COUNT; i++)
		{
			if (!passGPUTimeMetricID[i].IsValid()) continue;
			if (!gpuTimingFrame.passes[i].valid) continue;

			Debug::SubmitMetric(passGPUTimeMetricID[i], gpuTimingFrame.passes[i].milliseconds);
		}
	}
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

	RegisterDefaultDebugMetrics(); // ライブラリ標準機能のデバッグ設定

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
	cpuFrameTimingSystem.BeginFrame();  // 全体時間を計測開始
	{
		const auto timing{ Debug::BeginCPUTiming(debugBeginCpuMetricID) };
		DebugInternal::BeginFrame(); // デバッグ表示用のフレームの開始処理
	}
	{
		const auto timing{ Debug::BeginCPUTiming(debugMetricSubmitCpuMetricID) };
		SubmitCPUFrameTiming(); // Debugの書き込み開始時に前回CPU結果を提出
		SubmitPassDrawCount(); // 各パスのドローコール書き込み
	}
	TimeInternal::BeginFrame(); // 時間関連のフレーム最初の処理
	{
		const auto timing{ Debug::BeginCPUTiming(inputUpdateCpuMetricID) };
		InputInternal::BeginFrame(Time::UnscaledDeltaTime()); // 入力の最初の処理
	}
	{
		const auto timing{ Debug::BeginCPUTiming(uiUpdateCpuMetricID) };
		UIInternal::BeginFrame({ Time::UnscaledDeltaTime(), System::GetClientSize(), Gfx::GetVirtualSize() }); // 入力の更新後にUIの更新
	}
	{
		const auto timing{ Debug::BeginCPUTiming(cursorUpdateCpuMetricID) };
		SystemInternal::UpdateCursorLock(); // マウス移動量を取得した後に中央へ
	}
	{
		const auto timing{ Debug::BeginCPUTiming(graphicsBeginCpuMetricID) };
		GfxInternal::BeginFrame(); // グラフィックのフレーム最初の処理
	}
	{
		const auto timing{ Debug::BeginCPUTiming(debugMetricSubmitCpuMetricID) };
		SubmitGPUFrameTime(); // GPU完了済みフレームの時間を書き込む
	}
	{
		const auto timing{ Debug::BeginCPUTiming(soundUpdateCpuMetricID) };
		SoundInternal::BeginFrame(Time::UnscaledDeltaTime()); // 音関連のフレーム最初の処理
	}
}

void TSLib::EndFrame()
{
	SoundInternal::EndFrame(); // 音関連のフレーム最後の処理
	DebugInternal::EndFrame(); // デバッグ表示用のフレームの最後の処理
	DebugDraw(); // デバッグパラメータなどの描画
	GfxInternal::EndFrame(); // グラフィックのフレーム最後の処理
	InputInternal::EndFrame(); // 入力関連のフレーム最後の処理
	cpuFrameTimingSystem.EndFrame(Time::GetTargetFPS()); // 計測の終了
	TimeInternal::EndFrame(); // 時間関連のフレーム最後の処理(CPUの計測後にFPSの制限待機へ)
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
