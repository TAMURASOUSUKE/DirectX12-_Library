#pragma once
#include "../Core/Handle/SoundHandle.h"

// 音に関する機能をユーザーに提供する
namespace Sound
{
	// ファイルパスからハンドルを取得する
	SoundHandle LoadSound(const char* _filePath);
	// SEを再生する、初期音量を決める
	void PlaySE(SoundHandle _handle, float _volume = 1.0f);
	// BGMを再生する(boolはループさせるかどうかを決めます)(初期音量を決定する)
	void PlayBGM(SoundHandle _handle, bool _isLoop, float _volume = 1.0f);
	// BGMの再生を停止する
	void StopBGM();
	// BGMを破棄する
	void EndBGM();
	/// <summary>
	/// 特定の音のvolumeを調整する(音量は0-1に単位化されます)
	/// 現在生成済みのボイスを変更します。この関数の後に新たに生成された同ハンドルの音は初期音量が適用されます
	/// </summary>
	/// <param name="_handle">調整したい音のハンドル</param>
	/// <param name="_volume">音量</param>
	void SetVolume(SoundHandle _handle, float _volume);
	// 再生中も含め全ての音のボリュームを調整する(音量は0-1に単位化されます)
	void SetAllVolume(float _volume);
	// 再生中も含め全てのSEのボリュームを調整する(音量は0-1に単位化されます)
	void SetAllSEVolume(float _volume);
	// 再生中も含め全てのBGMのボリュームを調整する(音量は0-1に単位化されます)
	void SetAllBGMVolume(float _volume);
	// 全てのSEが再生終了しているか
	bool IsStopAllSE();
}