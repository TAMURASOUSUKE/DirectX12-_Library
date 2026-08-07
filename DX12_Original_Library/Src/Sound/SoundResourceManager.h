#pragma once
#include <vector>
#include "../Core/Handle/SoundHandle.h"
#include "SoundType.h"

// 音関連のリソース管理を集約するクラス
class SoundResourceManager
{
public:

	// シングルトン化
	static SoundResourceManager& Instance()
	{
		static SoundResourceManager instance;
		return instance;
	}

	void Setup(); // 引数なし、戻り値なしの理由はslotのサイズを制限してダングリングを防ぐためのreserveしかないから

	// ファイル名をもとにハンドルを返す
	SoundHandle LoadSound(const char* _filePath);

	// ハンドルを分解してDataの部分だけ取り出す関数
	SoundData* Lookup(SoundHandle _handle);

private:
	// コンストラクタ
	SoundResourceManager() = default;
	// コピー禁止
	SoundResourceManager(const SoundResourceManager& _other) = delete;
	SoundResourceManager& operator =(const SoundResourceManager& _other) = delete;

private:
	std::vector<SoundSlot> slots{}; // サウンドリソースのスロット

};
