#pragma once
#include <windows.h>
#include <functional>

// デバッグようにこれらをいったんincludeしているが3D機能拡張によって外す
#include "../Graphics/GraphicsType.h"
#include "../Component/Transform.h"

// グラフィックに関する関数のInitialize等ユーザーに提供しない部分をまとめた関数
namespace GfxInternal
{
	// 初期化(ウィンドウのタイトルと幅と高さを設定)
	bool Initialize(HWND _hwnd,  int _clientWidth, int _clientHeight, int _virtualWidth, int _virtualHeight);
	// フレームの開始処理
	void BeginFrame();
	// フレームの終了処理
	void EndFrame();
	// 終了処理
	void Finish();

	// アニメーションテストようにこっちに持ってきているが今後3D機能の拡張によってアニメーションの機能を付けてGfxへ
	void DrawAnimationModel(Transform _transform, AnimInstanceData& _animData);
}
