#include "DefaultFontData.h"

namespace InternalResource
{
	const std::uint8_t defaultFontPng[]
	{
		// DefaultFontAtlasのバイト列
		#include "DefaultFontBytes.inc"
	};

	const std::size_t defaultFontPngSize{ sizeof(defaultFontPng) };
}
