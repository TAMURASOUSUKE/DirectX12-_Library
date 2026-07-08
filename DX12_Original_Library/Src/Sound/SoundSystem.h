#pragma once
#include <wrl/client.h>
#include <xaudio2.h>
using Microsoft::WRL::ComPtr;

// 音に関するシステムを提供する
class SoundSystem
{
public:
	// XAudio2などの準備を行う
	bool Setup();
	void Cleanup(); // 解放処理

private:
	ComPtr<IXAudio2> audioEngine{ nullptr }; // 本体エンジン
	IXAudio2MasteringVoice* masterVoice{ nullptr }; // 最終出力となるマスターボイス

};