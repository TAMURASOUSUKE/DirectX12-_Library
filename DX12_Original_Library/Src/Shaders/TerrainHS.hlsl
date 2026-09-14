#include "Terrain.hlsli" // 構造体をまとめてあるファイルを持ってくる


// パッチ全体に対して一回呼ばれる関数
TerrainPatchConstant CalcPatch(InputPatch<TerrainControlPoint, 3> _patch)
{
	TerrainPatchConstant output;
	
	// 一旦全編同じ係数
	output.edge[0] = tessFactor; // Patch[1] - Patch[2]
	output.edge[1] = tessFactor; // Patch[2] - Patch[0]
	output.edge[2] = tessFactor; // Patch[0] - Patch[1]
	
	output.inside = tessFactor;
	return output;
}

// 三角形領域を分割する
[domain("tri")]
// 整数段階で分割数を変える
[partitioning("integer")]
// Tessellatorが生成する三角形の頂点順(まだCullMode = Noneなのでcw/ccwどちらでもよい)
[outputtopology("triangle_cw")]
// 1パッチから出力する制御点数
[outputcontrolpoints(3)]
// パッチ単位の分割係数を計算する関数
[patchconstantfunc("CalcPatch")]

// 出力制御点ごとに呼ばれるエントリーポイント
TerrainControlPoint main(InputPatch<TerrainControlPoint, 3> _patch, uint controlPointID : SV_OutputControlPointID)
{
	return _patch[controlPointID]; // そのままDSへ
}

