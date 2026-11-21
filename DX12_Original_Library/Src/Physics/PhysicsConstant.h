#pragma once

// 物理に関する定数を定義

constexpr int MAX_BODY_COUNT{ 4096 }; // 登録できるBodyの上限
constexpr int MAX_CONTACT_COUNT{ 4096 }; // 1回のStepで記録する接触の上限
constexpr int MAX_TRIANGLE_MESH_COUNT{ 1024 }; // 登録できる三角形の集まりの上限
constexpr float GRAVITY_Y{ -9.8f };  // 重力加速度
constexpr int SOLVER_ITERATION_COUNT{ 8 }; // 速度の解決をくり返す回数
constexpr float PENETRATION_SLOP{ 0.005f }; // この深さまでは戻さない(震えを防ぐ)
constexpr float PENETRATION_CORRECTION_RATE{ 0.8f }; // 1回のStepで戻すめり込みの割合
