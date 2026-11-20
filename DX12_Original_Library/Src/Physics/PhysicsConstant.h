#pragma once

// 物理に関する定数を定義

constexpr int MAX_BODY_COUNT{ 4096 }; // 登録できるBodyの上限
constexpr int MAX_CONTACT_COUNT{ 4096 }; // 1回のStepで記録する接触の上限
constexpr float GRAVITY_Y{ -9.8f };  // 重力加速度
