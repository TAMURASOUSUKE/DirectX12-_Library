#include <windows.h>
#include "../Debug/DebugLogs.h"
#include "SoundSystem.h"
#pragma comment(lib, "xaudio2.lib")

bool SoundSystem::Setup()
{
	HRESULT result{}; // 結果判定用
	result = XAudio2Create(&audioEngine, 0, XAUDIO2_DEFAULT_PROCESSOR); // エンジンを初期化する(作製する)
	DEBUG_ASSERT(SUCCEEDED(result) && "XAudioの初期化に失敗しました\n");
	if (FAILED(result)) return false;

	// スピーカーへの最終出力を作成
	result = audioEngine->CreateMasteringVoice(&masterVoice);
	DEBUG_ASSERT(SUCCEEDED(result) && "XAudioの最終出力の設定に失敗しました\n");
	if (FAILED(result)) return false;
	
	return true;
}

void SoundSystem:: Cleanup()
{
	 if (masterVoice)
    {
        masterVoice->DestroyVoice();
        masterVoice = nullptr;
    }

    if (audioEngine)
    {
		// デストラクタではなく明示的にここで廃棄することでCoUnInitializeするより前に破棄できる
		audioEngine.Reset();
    }
}