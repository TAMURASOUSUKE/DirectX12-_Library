#pragma once
#include <format>
#include "Vector3.h"

// デバッグ表示のためのvector3format
template<>
struct std::formatter<Vector3> : std::formatter<float> // float型のspecを使う
{
	auto format(const Vector3& _v, std::format_context& _ctx) const
	{
		auto out{ std::format_to(_ctx.out(), "(") }; // 開始【

		// x
		_ctx.advance_to(out); // ctxの更新
		out = std::formatter<float>::format(_v.x, _ctx); // 継承したfloatのspecから

		// y
		out = std::format_to(out, ", ");
		_ctx.advance_to(out);
		out = std::formatter<float>::format(_v.y, _ctx);

		// z
		out = std::format_to(out, ", ");
		_ctx.advance_to(out);
		out = std::formatter<float>::format(_v.z, _ctx);

		return std::format_to(out, ")"); // 最後に閉じる
	}
};