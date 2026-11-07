#pragma once
#include <d3d12.h>
#include "../Core/Handle/MaterialHandle.h"
#include "../Math/Vector/Vector3.h"

// VFXMeshSystemへ登録された描画単位(1メッシュ = Draw1回)
struct VFXDrawPacket
{
	UINT vertexStart{ 0 }; // 現在フレームの頂点領域の中での開始位置(頂点数単位)
	UINT vertexCount{ 0 }; // TRIANGLESTRIPの頂点数(板なら4、帯なら点の数×2)
	Vector3 sortPosition{ Vector3::Zero }; // 並べ替えの代表位置(頂点の重心)
	float sortDepth{ 0.0f }; // Flushで計算するカメラ距離の二乗(奥から手前に描くため)
	MaterialHandle material{}; // 登録はユーザーコードの途中で起きるためポインタではなくHandleで持ちFlushで検証する
};
