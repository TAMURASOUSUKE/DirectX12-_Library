#include <windows.h>
#include <xaudio2.h>]
#include <cstring>
#include <fstream>
#include "../Debug/DebugLogs.h"
#include "../Core/Handle/SoundHandle.h"
#include "SoundConstant.h"
#include "SoundType.h"
#include "SoundResourceManager.h"
#pragma comment(lib, "xaudio2.lib")

void SoundResourceManager::Initialize()
{
	slots.reserve(MAX_SOUND_COUNT); // あらかじめサイズを確保しておく + これ以上のサイズになることをLookup側で防ぐことによって確保位置の移動を行わせないようにしてタングリングを防止する
}

SoundHandle SoundResourceManager::LoadSound(const char* _filePath)
{
	DEBUG_ASSERT((slots.size() < MAX_SOUND_COUNT) && "SoundのLookupのSlotがサイズを超過しています\n");
	if (slots.size() >= MAX_SOUND_COUNT) return SoundHandle{}; // サイズを超過してれば空を返す(タングリング防止)

	std::fstream filePath{ _filePath, std::ios::in | std::ios::binary }; // 読み込み専用のバイナリモードで読む(テキストと誤認されないように)
	DEBUG_ASSERT(filePath.is_open() && "サウンドロードのファイルを開く処理に失敗しました\n");
	if (!filePath.is_open()) return SoundHandle{}; // 開くのにしっぱしたら無効ハンドルを返す

	char riff[4];
	char wave[4];
	filePath.read(riff, 4);  // riffに先頭アドレスから4バイト分読みこむ
	DEBUG_ASSERT((strncmp(riff, "RIFF", 4) == 0) && "RIFFフォーマットではありません\n");
	if (strncmp(riff, "RIFF", 4) != 0) return SoundHandle{}; // RIFFかどうか(0なら完全一致)
	filePath.seekg(8, std::ios::beg); // ファイルの先頭から8バイト目にシークする
	filePath.read(wave, 4); // waveに8-11バイト目を読み込む
	DEBUG_ASSERT((strncmp(wave, "WAVE", 4) == 0) && "WAVEファイルではありません\n");
	if (strncmp(wave, "WAVE", 4) != 0) return SoundHandle{};
}

SoundData* SoundResourceManager::Lookup(SoundHandle _handle)
{

	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}
	int packed{ _handle.GetRaw(PassKey{}) }; // 内部ハンドルを取り出す
	int index{ UnpackIndex(packed) }; // index取り出し
	if (index < 0 || index >= static_cast<int>(slots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	SoundSlot& slot{ slots[index] };
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data;
}