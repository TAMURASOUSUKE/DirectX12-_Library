#pragma once
#include <vector>
#include <xaudio2.h>
#include "../Math/TSMath.h"
#include "SoundConstant.h"


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
	std::vector<SoundData> data;
	std::vector<uint32_t> generation{ 0 }; // 音の解放はFinish関数で行うためUnloadの概念が存在しないが今後固定ボイスループなどのシステムの拡張や設計変更を行う際に耐えられるように0で埋めておく
};