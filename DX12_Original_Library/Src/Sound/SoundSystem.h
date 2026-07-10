#pragma once
#include <wrl/client.h>
#include <xaudio2.h>
#include <vector>
#include "../Core/Handle/SoundHandle.h"
#include "SoundType.h"
using Microsoft::WRL::ComPtr;

// 音に関するシステムを提供する
class SoundSystem
{
public:
	// XAudio2などの準備を行う
	bool Setup();
	// ハンドルをもとにSEを再生する
	void PlaySE(SoundHandle _handle, float _volume = 1.0f);
	// ハンドルをもとにBGMを再生する(bool = ループさせるかどうか)
	void PlayBGM(SoundHandle _handle, bool _isLoop, float _volume = 1.0f);
	// BGMの再生を停止する
	void StopBGM();
	// BGMを破棄する
	void EndBGM();
	// 特定の音のvolumeを調整する(音量は0-1に単位化されます)
	void SetVolume(SoundHandle _handle,  float _volume);
	// 全ての音のボリュームを調整する(音量は0-1に単位化されます)
	void SetAllVolume(float _volume);
	// 全てのSEのボリュームを調整する(音量は0-1に単位化されます)
	void SetAllSEVolume(float _volume);
	// 全てのBGMのボリュームを調整する(音量は0-1に単位化されます)
	void SetAllBGMVolume(float _volume);
	// フレーム最後に死んでいるボイス、更新される前のBGMを掃除する
	void EndFrameCleanup();
	// 全てのSEが再生終了しているか
	bool IsStopAllSE();
	// 解放処理
	void Cleanup();

private:
	ComPtr<IXAudio2> audioEngine{ nullptr }; // 本体エンジン
	IXAudio2MasteringVoice* masterVoice{ nullptr }; // 最終出力となるマスターボイス
	std::vector<SoundPair> liveVoices{}; // ボイスの台帳

	// クロスフェード用にcurrentとprevを用意する
	SoundPair currentBGM{}; // 現在のBGM
	SoundPair prevBGM{}; // 前のBGM

	// カテゴリ音量
	float seVolume{ 1.0f };
	float bgmVolume{ 1.0f };
};