#include <windows.h>
#include <xaudio2.h>
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
	slots.reserve(MAX_SOUND_COUNT); // あらかじめサイズを確保しておく + これ以上のサイズになることをLoad側で防ぐことによって確保位置の移動を行わせないようにしてダングリングを防止する
}

SoundHandle SoundResourceManager::LoadSound(const char* _filePath)
{
	DEBUG_ASSERT((slots.size() < MAX_SOUND_COUNT) && "SoundのLoadのSlotがサイズを超過しています\n");
	if (slots.size() >= MAX_SOUND_COUNT) return SoundHandle{}; // サイズを超過してれば空を返す(タングリング防止)

	std::ifstream file{ _filePath, std::ios::in | std::ios::binary }; // 読み込み専用のバイナリモードで読む(テキストと誤認されないように)
	DEBUG_ASSERT(file.is_open() && "サウンドロードのファイルを開く処理に失敗しました\n");
	if (!file.is_open()) return SoundHandle{}; // 開くのにしっぱしたら無効ハンドルを返す

	char id[4];
	uint32_t size{}; // サイズ(数値なのでuint)
	char wave[4];
	file.read(id, 4);  // idに先頭アドレスから4バイト分読みこむ
	DEBUG_ASSERT((memcmp(id, "RIFF", 4) == 0) && "RIFFフォーマットではありません\n"); // チャンクIDは生4バイトなのでmemcmp
	if (memcmp(id, "RIFF", 4) != 0) return SoundHandle{}; // RIFFかどうか(0なら完全一致)
	file.read(reinterpret_cast<char*>(&size), 4); // サイズ分数値で読む
	file.read(wave, 4); // waveに8-11バイト目を読み込む
	DEBUG_ASSERT((memcmp(wave, "WAVE", 4) == 0) && "WAVEファイルではありません\n");
	if (memcmp(wave, "WAVE", 4) != 0) return SoundHandle{};

	SoundData sd{};
	// チャンクの見出し部分を読むためIDとチャンクのサイズの見出し部分(8バイト)を読み続ける
	while (file.read(id, 4) && file.read(reinterpret_cast<char*>(&size), 4))
	{

		/*
			sizeは見出し(id)を読み込むたびに意味が変わることに留意
		*/

		if (memcmp(id, "fmt ", 4) == 0)
		{
			DEBUG_ASSERT((size >= 16) && "チャンクサイズが16バイト未満のファイルがありました\n");
			if (size < 16) return SoundHandle{}; // サイズが16バイト未満なら壊れたファイルとみなす
			// formatであるかを確認してそうであれば読み込み
			file.read(reinterpret_cast<char*>(&sd.wavefmt), 16); // 16バイト分
			if (size > 16)
			{
				file.seekg(size - 16, std::ios::cur); // 16バイト超過分シークしてフォーマット部分を終わらせる
			}
		}
		else if (memcmp(id, "data", 4) == 0)
		{
			// data(本体)の場合
			sd.data.resize(size); // サイズ分resizeする(この後size分readしたいため)
			file.read(reinterpret_cast<char*>(sd.data.data()), size); // dataの先頭からサイズ分だけ
		}
		else
		{
			// fmt やdata以外であればsize分シークする
			file.seekg(size, std::ios::cur);
		}
	}

	// 各種検問
	DEBUG_ASSERT((sd.wavefmt.wFormatTag == 1) && "フォーマットがありませんでした\n");
	if(sd.wavefmt.wFormatTag != 1) return SoundHandle{};
	DEBUG_ASSERT((sd.wavefmt.wBitsPerSample == 16) && "PMC16bitではありませんでした\n");
	if (sd.wavefmt.wBitsPerSample != 16) return SoundHandle{};
	DEBUG_ASSERT((!sd.data.empty()) && "dataが不在です\n");
	if (sd.data.empty()) return SoundHandle{};

	// データ照合
	DEBUG_LOG("1秒間に図る回数 : {}\n", sd.wavefmt.nSamplesPerSec);
	DEBUG_LOG("チャンネル数 : {}\n", sd.wavefmt.nChannels);
	DEBUG_LOG("1回の測定値を何bitで書くか : {}\n", sd.wavefmt.wBitsPerSample);
	DEBUG_LOG("圧縮なしの生PCM整数か : {}\n", sd.wavefmt.wFormatTag);
	DEBUG_LOG("曲全体のサイズ : {}\n", sd.data.size());
	DEBUG_LOG("音の長さ : {}\n", static_cast<float>(sd.data.size()) / sd.wavefmt.nAvgBytesPerSec); // 1標本(2byte) * 1フレームの全チャンネル(2byte) * 1秒で測る回数(44100)これに秒数を掛けるとdataサイズになるのでサイズを3つの要素を掛け合わせた物で割る


	// 作られたSoundDataをSlotsの中に入れる
	int index{ static_cast<int>(slots.size()) }; // 押し込む前のsize
	slots.emplace_back(); // 末尾に空を追加(move削減用)
	slots.back().data = std::move(sd);
	slots.back().generation = 0;
	return SoundHandle{ PassKey{}, Pack(index, slots.back().generation) }; // 世代は常に0

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