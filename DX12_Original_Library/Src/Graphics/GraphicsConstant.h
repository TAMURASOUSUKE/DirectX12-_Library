#pragma once
#include <windows.h>

// DX12の初期化や描画に関する定数を作成する
constexpr int HEAP_COUNT{ 3 }; // 用意するヒープの数
constexpr UINT FRAME_BUFFER_COUNT{ 2 }; // バッファ数
constexpr UINT SHADERVISIBLE_SLOT_COUNT{ 1024 }; // GPU可視のスロット数
constexpr UINT RTV_SLOT_COUNT{ 32 }; // GPU非可視のスロット数(RenderTargetView)
constexpr UINT DSV_SLOT_COUNT{ 16 }; // GPU非可視のスロット数(DepthStencilView)