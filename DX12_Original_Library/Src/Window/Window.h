#pragma once
#include <windows.h>
#include <functional>
#include <string>
#include "../Math/Vector/Vector2Int.h"


// ウィンドウ作成を行うクラス
class Window
{
public:
	// ウィンドウ作成 : 通常のタイトルバー付き
	bool GenerateWindow(int _clientWidth, int _clientHeight);
	// プライマリモニター全体を覆うタイトルバー無しWindow
	bool GenerateBorderlessFullscreen();
	// ウィンドウと登録したClassを破棄する
	void Shutdown();

	// ウィドウタイトルを設定する生成前なら内部保存して生成後ならタイトルバー反映
	bool SetWindowTitle(const wchar_t* _title);
	// タイトルバーを除いた描画領域を取得する
	Vector2Int GetClientSize() const;
	// 現在このウィンドウが操作対象になっているか
	bool IsFocused() const;
	// 通常の×ボタンと同じ終了経路を要求する
	void RequestQuit();
	// オブザーバーパターンの監視者される側として値を伝えるためのコールバック
	void SetOnWheel(std::function<void(short)> _func) { onWheel = _func; }

	HWND GetHWND() const { return hwnd; } // ウィンドウハンドルの取得

private:
	// 計算済みのWindowStyle、位置、外側サイズからWin32Windowを生成する
	bool GenerateNativeWindow(DWORD _windowStyle, int _x, int _y, int _width, int _height);

	static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp); // カスタムのプロシージャ
 private:
	 std::function<void(short)> onWheel{}; // 回転量計算用
	 HWND hwnd{ nullptr }; // ウィンドウハンドル
	 std::wstring windowTitle{ L"DefaultWindow" }; // ウィンドウの名前(wstringにすることで呼び出し側の文字列寿命に依存しない)

};
