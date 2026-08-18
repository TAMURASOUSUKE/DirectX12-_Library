#pragma once

// 入力に関する定数を定義

// 0x80 = GetKeyboardStateにより取得される最上位ビット
constexpr int MOST_SIGNIFICANT_BIT{ 0x80 }; 
// スティックから取得できる最大値
constexpr float MAX_STICK_VALUE{ 32767.0f };  // スティックを倒したときの最大値
constexpr float MAX_TRIGGER_VALUE{255.0f}; // トリガーを押し切った場合の最大値
constexpr float RIGHT_STICK_DEADZONE{ 8689.0f }; // 右スティックのデッドゾーン
constexpr float LEFT_STICK_DEADZONE{ 7849.0f }; // 左スティックのデッドゾーン
constexpr float TRIGGER_THRESHOLD{ 30.0f }; // トリガーの閾値
constexpr float TRIGGER_RELEASE_THRESHOLD{ 10.0f }; // ヒステリシスとして離す時用の閾値を設ける
constexpr int WHEEL_DELTA_VALUE{ 120 }; // wheelを回転させたときの基準値
constexpr int MOUSE_MOVE_THRESHOLD_PER_FRAME{ 10 }; // マウスが1フレームに何px動いたらマウス操作をしていると判定するか
