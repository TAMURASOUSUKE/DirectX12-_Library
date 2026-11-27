#pragma once
#include <cstddef>

// 衝突に関する定数
constexpr std::size_t MAX_CONTACT_POINT_COUNT{ 4 }; // 衝突点の最大個数
constexpr int SWEEP_ITERATION_COUNT{ 32 }; // 進めるのをくり返す回数の上限
constexpr float SWEEP_TOLERANCE{ 0.0001f }; // 隙間がこれ以下なら触れたとみなす
