#pragma once
#include <cstddef>
#include <cstdint>

// ライブラリ内部に埋め込むデフォルトフォント画像
namespace InternalResource
{
	// externconstによって他cppからも実体を参照できる
	extern const std::uint8_t defaultFontPng[];
	extern const std::size_t defaultFontPngSize;
}
