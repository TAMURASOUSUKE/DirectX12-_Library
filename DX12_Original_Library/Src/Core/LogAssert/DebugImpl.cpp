#include <windows.h>
#include <intrin.h>
#include "DebugImpl.h"

// 内部実装を行う
void Debug::Detail::Write(const std::string& _msg)
{
	OutputDebugStringA(_msg.c_str()); // 出力
}

void Debug::Detail::AssertImpl(bool _cond, const char* _expr, const char* _file, int _line)
{
	if (_cond) return; // 成功していたら何もしない
	Write(std::format("[Assert] failed: {} ({} : {})\n", _expr, _file, _line));
	__debugbreak(); // ここで終了する(デバッガで止める)
}