#include <windows.h>
#include <intrin.h>
#include "DebugLogs.h"

// 内部実装を行う
void Debug::Detail::Write(const std::string& _msg)
{
	if (_msg.empty()) return;

	// UTF-8からUTF-16へ変換するために必要な文字数を取得する
	// -1を渡しているので終端のnull文字も計算に含まれる
	const int wideLength{ MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, _msg.c_str(), -1, nullptr, 0) };
	if (wideLength <= 0)
	{
		// 変換に失敗したことだけはUTF-16文字列で知らせる
		OutputDebugStringW(L"[DebugLog] UTF-8からUTF-16への変換に失敗しました\n");
		return;
	}

	// 終端null文字を含む大きさでUTF-16文字列を確保する
	std::wstring wideMessage(static_cast<std::size_t>(wideLength), L'\0');

	// UTF-8からUTF-16への変換
	const int convertedLength{ MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, _msg.c_str(), -1, wideMessage.data(), wideLength) };
	if (convertedLength <= 0)
	{
		OutputDebugStringW(L"[DebugLog] UTF-8からUTF-16への変換に失敗しました\n");
		return;
	}
	// WindowsへはUTF-16として渡す
	OutputDebugStringW(wideMessage.c_str()); // 出力
}

// Assert内部実装
void Debug::Detail::AssertImpl(bool _cond, const char* _expr, const char* _file, int _line)
{
	if (_cond) return; // 成功していたら何もしない
	Write(std::format("[Assert] failed: {} ({} : {})\n", _expr, _file, _line));
	__debugbreak(); // ここで終了する(デバッガで止める)
}
