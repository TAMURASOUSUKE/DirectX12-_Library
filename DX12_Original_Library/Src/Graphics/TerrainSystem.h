#pragma once
#include "RingConstantBuffer.h"
#include "GraphicsConstant.h"

// Terrainによる地形描画を担当するシステム
class TerrainSystem
{
public:

	// 初期化関数
	bool Setup();

	// 終了処理
	void Shutdown();

	// 描画関数
	void Draw();

private:
	RingConstantBuffer ringCBV{};


};
