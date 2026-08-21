#pragma once

// UIに関するユーザーに公開しない内部関数を定義する
namespace UIInternal
{
	// 初期化
	void Initialize();
	// 終了処理
	void Finish();
	// フレーム最初の処理
	void BeginFrame();
}
