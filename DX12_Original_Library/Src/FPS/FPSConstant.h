#pragma once

// 1秒をマイクロ秒に変換した値
constexpr long long MICROSECONDS_PER_SECOND{ 1'000'000LL };

// 処理落ちした際にDeltaTimeが大きくなりすぎるのを防ぐ上限値
constexpr float MAX_DELTA_TIME{ 0.05f };

// 固定更新の間隔
constexpr float FIXED_DELTA_TIME{ 1.0f / 60.0f };

// 固定更新が一度に大量実行されるのを防ぐ蓄積時間の上限
constexpr float MAX_FIXED_ACCUMULATOR{ 0.2f };