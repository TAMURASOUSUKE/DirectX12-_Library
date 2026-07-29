#include <windows.h>
#include "../Debug/DebugLogs.h"
#include "GfxInternal.h"
#include "InputInternal.h"
#include "SoundInternal.h"
#include "TimeInternal.h"
#include "SystemInternal.h"
#include "TSLib.h"

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
	result = InputInternal::Initialize(SystemInternal::GetHWND());
	DEBUG_ASSERT(result && "入力処理の初期化に失敗しました\n");
	if (!result) return result;
	result = SoundInternal::Initialize(); // XAudio2はCoInitializeに依存するため初期化が行われるGfxの後に初期化
	DEBUG_ASSERT(result && "音処理の初期化に失敗しました\n");
	if (!result) return result;

	// コールバックの配線接続 : ラムダで渡す
	SystemInternal::SetOnWheel([](short _d) { InputInternal::AddMouseWheelDelta(_d); });

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
	TimeInternal::BeginFrame(); // 時間関連のフレーム最初の処理
	InputInternal::BeginFrame(); // 入力の最初の処理
	GfxInternal::BeginFrame(); // グラフィックのフレーム最初の処理
	SoundInternal::BeginFrame(Time::UnscaledDeltaTime()); // 音関連のフレーム最初の処理
}

void TSLib::EndFrame()
{
	SoundInternal::EndFrame(); // 音関連のフレーム最後の処理
	GfxInternal::EndFrame(); // グラフィックのフレーム最後の処理
	InputInternal::EndFrame(); // 入力関連のフレーム最後の処理
	TimeInternal::EndFrame(); // 時間関連のフレーム最後の処理
}

void TSLib::Finish()
{
	SoundInternal::Finish(); // 音の終了処理
	InputInternal::Finish(); // 入力の終了処理
	GfxInternal::Finish(); // グラフィックの終了処理
	SystemInternal::Finish(); // システムの終了処理
	TimeInternal::Finish(); // 時間管理の終了処理
	CoUninitialize(); // COMも閉じる
}
