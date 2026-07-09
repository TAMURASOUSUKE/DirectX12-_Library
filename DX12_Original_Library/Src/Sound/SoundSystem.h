#pragma once
#include <wrl/client.h>
#include <xaudio2.h>
#include <vector>
#include "../Core/Handle/SoundHandle.h"
using Microsoft::WRL::ComPtr;

// 音に関するシステムを提供する
class SoundSystem
{
public:
	// XAudio2などの準備を行う
	bool Setup();
	// ハンドルをもとにSEを再生する
	void PlaySE(SoundHandle _handle);
	// フレーム最後に死んでいるボイスを掃除する
	void EndFrameCleanup();
	// 解放処理
	void Cleanup();

private:
	ComPtr<IXAudio2> audioEngine{ nullptr }; // 本体エンジン
	IXAudio2MasteringVoice* masterVoice{ nullptr }; // 最終出力となるマスターボイス
	std::vector<IXAudio2SourceVoice*> liveVoices{}; // ボイスの台帳

};