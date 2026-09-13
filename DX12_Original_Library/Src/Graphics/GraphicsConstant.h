#pragma once
#include <windows.h>
#include "GfxType.h"

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
constexpr size_t MAX_PRIMITIVE_3D_INSTANCE_COUNT{ 8192 }; // 1フレームに登録できる3D基礎図形インスタンスの総数
constexpr size_t CIRCLE_DIVISION{ 32 }; // 円を描画するときの三角形分割数
constexpr size_t MAX_CB_PER_FRAME{ 256 }; // フレーム内で使える定数バッファの最大数
constexpr size_t MAX_BONE_NUM{ 256 }; // 最大ボーン数
constexpr size_t MAX_TEXTURE_COUNT{ 1024 }; // 最大画像ロード数
constexpr size_t MAX_MODEL_COUNT{ 2048 }; // 最大モデルロード数
constexpr size_t MAX_MODEL_RENDER_COMMAND_COUNT{ 256 }; // 1フレーム内で可能な描画命令登録数
constexpr std::size_t MAX_MODEL_DRAW_PACKET_COUNT{ 2048 }; // 全サブメッシュの上限
constexpr UINT SHADOW_MAP_RESOLUTION{ 2048 }; // 影の解像度
constexpr UINT INVALID_INDEX{ UINT_MAX }; // Descriptorハンドルのindex無効値
constexpr size_t MAX_RENDER_TARGET_COUNT{ 16 }; // 登録できるRenderTargetの最大数
constexpr size_t MAX_CUSTOM_SHADER_COUNT{ 64 }; // 登録できるShaderの最大数
constexpr size_t MAX_MATERIAL_COUNT{ 64 }; // 登録できるmaterialの最大数
constexpr size_t MATERIAL_PARAMETER_SLOT_COUNT{ 4 }; // materialが保持できるユーザーパラメータスロット数
constexpr UINT MATERIAL_PARAMETER_REGISTER_BASE{ 0 }; // ユーザーパラメータが使用するHLSL側の先頭レジスタのベース番号
constexpr UINT USER_DEFINE_REGISTER_SPACE_NUM{ 1 }; // ユーザーが定義したマテリアルパラメータが置かれるレジスタースペース番号
constexpr size_t SPRITE_BATCH_COUNT{ 2 }; // 現在存在するspritebatchの数(backとforground)
constexpr size_t MAX_ANIM_INSTANCE_COUNT{ 1024 }; // 同時に存在できるアニメーション個体数
constexpr UINT PRIMITIVE_3D_SPHERE_SLICE_COUNT{ 32 }; // 3D球体の横方向分割数
constexpr UINT PRIMITIVE_3D_SPHERE_STACK_COUNT{ 16 }; // 3D球体の縦方向の分割数
constexpr UINT PRIMITIVE_3D_CYLINDER_DIVISION{ 32 }; // 3D円柱の円周方向分割数
// 1フレームで発生しうるMaterialParamerter更新の最大数
// 全Spriteが別Runかつ4スロット全使用する最悪条件にPostEffectの4スロットを追加する
constexpr size_t MAX_MATERIAL_PARAMETER_UPDATE_PER_FRAME{ MAX_SPRITE_COUNT * SPRITE_BATCH_COUNT * MATERIAL_PARAMETER_SLOT_COUNT + MATERIAL_PARAMETER_SLOT_COUNT };

