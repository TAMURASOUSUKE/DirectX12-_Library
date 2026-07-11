#pragma once
#include <vector>
#include <xaudio2.h>
#include "../Core/Handle/SoundHandle.h"


// Handle作製などに必要な型を定義する


// サウンドのデータ本体
struct SoundData
{
	WAVEFORMATEX wavefmt{}; // データを構成するフォーマット
	std::vector<uint8_t> data;  // 内部データ(PCMのバイト列)
};

// サウンドの管理を行うスロット
struct SoundSlot
{
	SoundData data;
	uint32_t generation{ 0 }; // 音の解放はFinish関数で行うためUnloadの概念が存在しないが今後固定ボイスループなどのシステムの拡張や設計変更を行う際に耐えられるように0で埋めておく
};

// ハンドルと音のデータをペアで管理する構造体 
struct SoundPair
{
	SoundHandle handle{};
	IXAudio2SourceVoice* voiceResource{ nullptr };

	float volume{ 1.0f };       // この音固有の音量
	float fadeVolume{ 1.0f };   // クロスフェード用
	bool isPaused{ false };     // BGM一時停止判定
};