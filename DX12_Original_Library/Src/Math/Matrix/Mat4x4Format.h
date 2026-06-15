#pragma once
#pragma once
#include <format>
#include "../Vector/Vector4Format.h"
#include "../MathConstant.h"
#include "Mat4x4.h"

// デバッグ表示のためのmat4x4format
template<>
struct std::formatter<Mat4x4> : std::formatter<Vector4> // vector4型のspecを使う
{
	auto format(const Mat4x4& _m, std::format_context& _ctx) const
	{
		auto out{ _ctx.out() }; // 出力位置

		// 各行をVector4型を継承して出力していく
		for (int i = 0; i < Math::MATRIX_SIZE; i++)
		{
			out = std::format_to(out, "\n "); // 先頭(改行 + インデント)
			_ctx.advance_to(out); // 出力位置の更新
			out = formatter<Vector4>::format(_m.rows[i], _ctx); // 一行丸ごと出す
		}

		return out;
	}
};