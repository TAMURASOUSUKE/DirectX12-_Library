#pragma once
#include <string>
#include <string_view>

// ライブラリ」内部で文字コードを変換する
// TSlibでインクルードしないのでSDK利用者には公開しない

namespace TextEncoding
{
	// UTF-16からUTF-8へ
	std::string ToUtf8(std::wstring_view _text);
}
