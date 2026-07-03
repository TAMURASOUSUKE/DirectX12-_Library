#pragma once

// 入力に関する定数を定義

// 0x80 = GetKeyboardStateにより取得される最上位ビット
constexpr int MOST_SIGNIFICANT_BIT{ 0x80 }; 
// スティックから取得できる最大値
constexpr float MAX_STICK_VALUE{ 32767 }; 
constexpr float RIGHT_STICK_DEADZONE{ 8689 }; // 右スティックのデッドゾーン
constexpr float LEFT_STICK_DEADZONE{ 7849 }; // 左スティックのでエッドゾーン