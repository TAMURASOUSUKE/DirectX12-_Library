#include "DefaultFontData.h"

namespace InternalResource
{
	const std::uint8_t defaultFontPng[]
	{
		// DejaVu Sans Mono.pngのバイト列
		#include "DefaultFontBytes.inc"
	};

	const std::size_t defaultFontPngSize{ sizeof(defaultFontPng) };
}
