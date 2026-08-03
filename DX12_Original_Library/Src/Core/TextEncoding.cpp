#include <windows.h>
#include"TextEncoding.h"

std::string TextEncoding::ToUtf8(std::wstring_view _text)
{
	if (_text.empty()) return {};

	// UTF-8へ変換した際に必要になるバイト数を取得する
	const int requiredSize{ WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, _text.data(), static_cast<int>(_text.size()), nullptr, 0, nullptr, nullptr) };
	if (requiredSize <= 0) return {};
	std::string result(static_cast<size_t>(requiredSize), '\0');
	// UTF-16からUTF-8へ変換する
	const int convertedSize{ WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, _text.data(), static_cast<int>(_text.size()), result.data(), requiredSize, nullptr, nullptr) };
	if (convertedSize <= 0) return {};
	return result;
}

std::wstring TextEncoding::ToUtf16(std::string_view _text)
{
	if (_text.empty()) return {};
	// UTF-16へ変換した際に必要な文字数
	const int requiredSize{ MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, _text.data(), static_cast<int>(_text.size()), nullptr, 0) };
	if (requiredSize <= 0) return {};
	std::wstring result(static_cast<size_t>(requiredSize), L'\0');
	// 実際にUTF-8からUTF-16へ変換する
	const int convertedSize{ MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, _text.data(), static_cast<int>(_text.size()), result.data(), requiredSize) };
	if (convertedSize <= 0) return {};
	return result;
}
