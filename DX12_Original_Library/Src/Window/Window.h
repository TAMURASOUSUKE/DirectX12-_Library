#pragma once
#include <windows.h>
#include <functional>
#include <utility>
#include <string>
#include "../Math/Vector/Vector2Int.h"


// ウィンドウ作成を行うクラス
class Window
{
public:
	// ウィンドウ作成 : 通常のタイトルバー付き
	bool GenerateWindow(int _clientWidth, int _clientHeight);
	// プライマリモニター全体を覆うタイトルバー無しWindow
	bool GenerateBorderlessFullscreen(int _windowedClientWidth, int _windowedClientHeight);
	// ウィンドウと登録したClassを破棄する
	void Shutdown();

	// 表示状態と固定状態をまとめて変更する
	bool SetCursorState(bool _visible, bool _locked);
	// Locked時にカーソルを中央へ戻す
	void UpdateCursorLock();

	// ウィドウタイトルを設定する生成前なら内部保存して生成後ならタイトルバー反映
	bool SetWindowTitle(const wchar_t* _title);
	// タイトルバーを除いた描画領域を取得する
	Vector2Int GetClientSize() const;
	// 現在このウィンドウが操作対象になっているか
	bool IsFocused() const;
	// 実行中にボーダーレスフルスクリーンを切り替える
	bool SetBorderlessFullscreen(bool _enabled);
	// 通常の×ボタンと同じ終了経路を要求する
	void RequestQuit();

	// EXEへ埋め込まれたアイコンを実行中のウィンドウへ適用する
	bool SetWindowIcon(int _resourceID);

	// オブザーバーパターンの監視者される側として値を伝えるためのコールバック
	void SetOnWheel(std::function<void(short)> _func) { onWheel = std::move( _func); }
	// クライアント領域のサイズ変更通知先を登録する
	void SetOnResize(std::function<void(int, int)> _func) { onResize = std::move(_func); }
	// ライブラリがカーソルを強制移動したことを通知する
	void SetOnCursorWarp(std::function<void()> _func) { onCursorWarp = std::move(_func); }

	// ウィンドウハンドルの取得
	HWND GetHWND() const { return hwnd; }
	// 現在ボーダーレスフルスクリーンか
	bool IsBorderlessFullscreen() const { return isBorderlessFullscreen; }
	// マウスが表示されているか
	bool IsCursorVisible() const { return cursorVisible; }
	// マウスが固定されているか
	bool IsCursorLocked() const { return cursorLocked; }

private:
	// 計算済みのWindowStyle、位置、外側サイズからWin32Windowを生成する
	bool GenerateNativeWindow(DWORD _windowStyle, int _x, int _y, int _width, int _height);
	// カスタムのプロシージャ
	static LRESULT CALLBACK WindowProc(HWND _hwnd, UINT _msg, WPARAM _wp, LPARAM _lp);

	// カーソルの状態を適用する
	bool ApplyCursorState(bool _isFocused);
	// カーソルを中央へ
	bool CenterCursor();
 private:
	 std::function<void(short)> onWheel{}; // 回転量計算用
	 std::function<void(int, int)> onResize{}; // ウィンドウサイズ変更用
	 std::function<void()> onCursorWarp; // ライブラリがカーソルを強制移動させた通知

	 HWND hwnd{ nullptr }; // ウィンドウハンドル
	 WINDOWPLACEMENT windowedPlacement{};  // フルスクリーン解除にもとの位置、サイズへ戻すための情報
	 LONG_PTR windowedStyle{ WS_OVERLAPPEDWINDOW & ~(WS_THICKFRAME | WS_MAXIMIZEBOX) }; // もとのWindowedスタイル

	 std::wstring windowTitle{ L"DefaultWindow" }; // ウィンドウの名前(wstringにすることで呼び出し側の文字列寿命に依存しない)

	 Vector2Int preferredWindowedClientSize{ Vector2Int::Zero }; // Borderless起動からWindowedへ戻る場合の基準サイズ

	 bool hasWindowedPlacement{ false }; // windowed状態を保存できているか
	 bool isBorderlessFullscreen{ false }; // 現在のモード
	 bool cursorVisible{ true }; // カーソルの可視状態
	 bool cursorLocked{ false }; // カーソルの固定状態

	 // LoadImageWで生成したアイコンを所有する
	 HICON largeIcon{ nullptr };
	 HICON smallIcon{ nullptr };
};
