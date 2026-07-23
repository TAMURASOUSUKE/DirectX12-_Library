#include "../GraphicsResourceManager.h"
#include "../GPUMarker.h"
#include "../GraphicsConstant.h"
#include "DebugCube.h"

void DebugCube::Initialize()
{
	// 頂点データの中身を作る
	ShapeVertex vertices[]
	{
		// 前面
		{{-0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 左上
		{{ 0.5f, 0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 右上
		{{ 0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 右下
		{{-0.5f, -0.5f, 0.5f}, {1.0f, 0.0f, 0.0f, 1.0f}}, // 左下
		// 裏面
		{{-0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 左上
		{{ 0.5f, 0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 右上
		{{ 0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 右下
		{{-0.5f, -0.5f, -0.5f}, {0.0f, 0.0f, 1.0f, 1.0f}}, // 左下
		// 右面
		{{ 0.5f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 1.0f}}, // 左上
		{{ 0.5f, 0.5f, -0.5f}, {0.5f, 1.0f, 0.5f, 1.0f}}, // 右上
		{{ 0.5f, -0.5f, -0.5f}, {0.5f, 1.0f, 0.5f, 1.0f}}, // 右下
		{{ 0.5f, -0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 1.0f}}, // 左下
		// 左面
		{{ -0.5f, 0.5f, -0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}}, // 左上
		{{ -0.5f, 0.5f, 0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}}, // 右上
		{{ -0.5f, -0.5f, 0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}}, // 右下
		{{ -0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 1.0f, 1.0f}}, // 左下
		// 上面
		{{ -0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // 左上
		{{ 0.5f, 0.5f, -0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // 右上
		{{ 0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // 右下
		{{ -0.5f, 0.5f, 0.5f}, {1.0f, 1.0f, 0.0f, 1.0f}}, // 左下
		// 下面
		{{ -0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 左上
		{{ 0.5f, -0.5f, -0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 右上
		{{ 0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 右下
		{{ -0.5f, -0.5f, 0.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 左下
	};

	UINT indexes[CUBE_VERT_INDEXES]; // 頂点インデックス用配列
	UINT indexesPattern[QUAD_VERT_INDEXES]{ 0, 1, 2, 0, 2, 3 }; // CubeはQuadが6面分ある状態なので一面分の頂点インデックスを用意する

	for (int i = 0; i < 6; i++) // 6面
	{
		UINT offset{ static_cast<UINT>(i) * 4 }; // 面ごとのオフセット
		for (int j = 0; j < 6; j++) // 6頂点
		{
			indexes[i * 6 + j] = indexesPattern[j] + offset;
		}
	}

	vertexBuffer = GraphicsResourceManager::Instance().CreateVertexBuffer(vertices, sizeof(vertices), sizeof(ShapeVertex)); // 頂点バッファの作成を行う
	indexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(indexes, sizeof(indexes), CUBE_VERT_INDEXES); // 頂点インデックスの作成を行う
}

void DebugCube::Draw(ID3D12GraphicsCommandList* _cmdList)
{
	GPU_MARKER("cube");
	if (_cmdList == nullptr) return;

	// 入力アセンブラを設定
	_cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // リスト設定
	_cmdList->IASetVertexBuffers(0, 1, &vertexBuffer.vertexView);
	_cmdList->IASetIndexBuffer(&indexBuffer.indexView);

	// インデックス描画
	_cmdList->DrawIndexedInstanced(indexBuffer.indexCount, 1, 0, 0, 0);
}

void DebugCube::SetRotation(Vector3 _angle)
{
	Quaternion angleQua{ Quaternion::FromEuler(_angle)};

	transform.SetRotation(angleQua);
}