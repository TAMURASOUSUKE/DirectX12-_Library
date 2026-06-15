#pragma once
#pragma once
#include <format>
#include "Quaternion.h"

// デバッグ表示のためのvector4format
template<>
struct std::formatter<Quaternion> : std::formatter<float> // float型のspecを使う
{
	auto format(const Quaternion& _q, std::format_context& _ctx) const
	{
		auto out{ std::format_to(_ctx.out(), "Quaternion : (") }; // 開始【

		// x
		_ctx.advance_to(out); // ctxの更新
		out = std::formatter<float>::format(_q.x, _ctx); // 継承したfloatのspecから

		// y
		out = std::format_to(out, ", ");
		_ctx.advance_to(out);
		out = std::formatter<float>::format(_q.y, _ctx);

		// z
		out = std::format_to(out, ", ");
		_ctx.advance_to(out);
		out = std::formatter<float>::format(_q.z, _ctx);

		// w
		out = std::format_to(out, ", ");
		_ctx.advance_to(out);
		out = std::formatter<float>::format(_q.w, _ctx);

		return std::format_to(out, ")"); // 最後に閉じる
	}
};