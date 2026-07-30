#pragma once
#include <string>
#include <string_view>

// ライブラリ内部で文字コードを変換する
// TSlibでインクルードしないのでSDK利用者には公開しない

namespace TextEncoding
{
	// UTF-16からUTF-8へ
	std::string ToUtf8(std::wstring_view _text);

	// UTF-8からUTF-16へ
	std::wstring ToUtf16(std::string_view _text);
}
