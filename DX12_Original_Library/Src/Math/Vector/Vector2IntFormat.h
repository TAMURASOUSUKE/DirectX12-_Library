#pragma once
#include <format>
#include "Vector2Int.h"

// デバッグ表示のためのvector2Intformat
template<>
struct std::formatter<Vector2Int> : std::formatter<int> // int型のspecを使う
{
	auto format(const Vector2Int& _v, std::format_context& _ctx) const
	{
		auto out{ std::format_to(_ctx.out(), "(") }; // 開始【

		// x
		_ctx.advance_to(out); // ctxの更新
		out = std::formatter<int>::format(_v.x, _ctx); // 継承したintのspecから

		// y
		out = std::format_to(out, ", ");
		_ctx.advance_to(out);
		out = std::formatter<int>::format(_v.y, _ctx);

		return std::format_to(out, ")"); // 最後に閉じる
	}
};