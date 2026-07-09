#pragma once
#include "../Core/Handle/SoundHandle.h"

// 音に関する機能をユーザーに提供する
namespace Sound
{
	// ファイルパスからハンドルを取得する
	SoundHandle LoadSound(const char* _filePath);
	// SEを再生する
	void PlaySE(SoundHandle _handle);


}