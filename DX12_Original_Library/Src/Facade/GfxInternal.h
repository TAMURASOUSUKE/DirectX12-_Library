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
	bool Initialize(const wchar_t* _title, int _windowWidth, int _windowHeight, int _virtualWidth, int _virtualHeight);
	// フレームの開始処理
	void BeginFrame();
	// フレームの終了処理
	void EndFrame();
	// 終了処理
	void Finish();


	// HWNDの取得(今後はsystemファサードの役目になる)
	HWND GetHWND();
	// ホイール関数のセット(今後はsystemファサードの役目になる)
	void SetOnWheel(std::function<void(short)> _func);

	// アニメーションテストようにこっちに持ってきているが今後3D機能の拡張によってアニメーションの機能を付けてGfxへ
	void DrawAnimationModel(Transform _transform, AnimInstanceData& _animData);
}
