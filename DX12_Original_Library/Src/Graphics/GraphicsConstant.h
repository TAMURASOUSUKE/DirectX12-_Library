#pragma once
#include <windows.h>

// DX12の初期化や描画に関する定数を作成する
constexpr size_t HEAP_COUNT{ 3 }; // 用意するヒープの数
constexpr size_t FRAME_BUFFER_COUNT{ 2 }; // バッファ数
constexpr size_t SHADERVISIBLE_SLOT_COUNT{ 2048 }; // GPU可視のスロット数
constexpr size_t RTV_SLOT_COUNT{ 32 }; // GPU非可視のスロット数(RenderTargetView)
constexpr size_t DSV_SLOT_COUNT{ 16 }; // GPU非可視のスロット数(DepthStencilView)
constexpr size_t QUAD_VERT_INDEXES{ 6 }; // Quadを描画するときの頂点インデックス数
constexpr size_t CUBE_VERT_INDEXES{ 36 }; // cubeを描画するときの頂点インデックス数
constexpr size_t MAX_SPRITE_COUNT{ 1024 }; // 登録できる最大のスプライト数
constexpr size_t MAX_SHAPE_COUNT{ 1024 }; // 登録できる最大の基礎画像数
constexpr size_t CIRCLE_DIVISION{ 32 }; // 円を描画するときの三角形分割数
constexpr size_t MAX_CB_PER_FRAME{ 256 }; // フレーム内で使える定数バッファの最大数
constexpr size_t MAX_BONE_NUM{ 256 }; // 最大ボーン数
constexpr size_t MAX_TEXTURE_COUNT{ 1024 }; // 最大画像ロード数
constexpr size_t MAX_MODEL_COUNT{ 2048 }; // 最大モデルロード数
constexpr UINT INVALID_INDEX{ UINT_MAX }; // Descriptorハンドルのindex無効値
constexpr size_t MAX_RENDER_TARGET_COUNT{ 16 }; // 登録できるRenderTargetの最大数
constexpr size_t MAX_CUSTOM_SHADER_COUNT{ 64 }; // 登録できるShaderの最大数
constexpr size_t MAX_MATERIAL_COUNT{ 64 }; // 登録できるmaterialの最大数
constexpr size_t MAX_MATERIAL_PARAMETER_SIZE{ 256 }; // materialへ設定できるパラメータの最大サイズ(GPUへ送る際は1スライス256byteとして扱う)
constexpr size_t MATERIAL_PARAMETER_SLOT_COUNT{ 4 }; // materialが保持できるユーザーパラメータスロット数
constexpr UINT MATERIAL_PARAMETER_REGISTER_BASE{ 4 }; // ユーザーパラメータが使用するHLSL側の先頭レジスタslot0 = b4, slot1 = b5...となる