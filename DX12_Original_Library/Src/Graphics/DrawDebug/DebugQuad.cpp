#include "../GraphicsConstant.h"
#include "../ResourceManager.h"
#include "../DescriptorManager.h"
#include "DebugQuad.h"


// 初期化
void DebugQuad::Initialize()
{
	// 頂点データの中身を作る
	TexVertex vertices[]
	{	
		{{100.0f,  100.0f, 0.0f}, {0.0f, 0.0f}}, // 左上
		{{ 300.0f,  100.0f, 0.0f}, {1.0f, 0.0f}}, // 右上
		{{ 300.0f, 300.0f, 0.0f}, {1.0f, 1.0f}}, // 右下
		{{100.0f, 300.0f, 0.0f}, {0.0f, 1.0f}}, // 左下
	};

	// 頂点データから頂点インデックスの中身を作る(左手系なので時計回りに設定する)
	UINT indexes[QUAD_VERT_INDEXES]{ 0, 1, 2, 0, 2, 3 };

	vertexBuffer = ResourceManager::Instance().CreateVertexBuffer(vertices, sizeof(vertices), sizeof(TexVertex)); // 頂点バッファの作成を行う
	indexBuffer = ResourceManager::Instance().CreateIndexBuffer(indexes, sizeof(indexes), QUAD_VERT_INDEXES); // 頂点インデックスの作成を行う
	textureData = ResourceManager::Instance().LoadTexture("Res/enemy.png"); // テクスチャをロードする

}

// 描画命令
void DebugQuad::Draw(ID3D12GraphicsCommandList* _cmdList)
{
	if (_cmdList == nullptr) return;
	if (!textureData.IsValid()) return;

	TextureData* data{ ResourceManager::Instance().Lookup(textureData) };

	if (!data) return;

	// SRVが入っているDescriptorHeapをGPUにセットする
	DescriptorManager::Instance().SetDiscriptor(_cmdList);

	// ルートシグネチャの0番にテクスチャのGPUハンドルをセット
	_cmdList->SetGraphicsRootDescriptorTable(0, data->srvHandle.gpu);

	// 入力アセンブラを設定
	_cmdList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // リスト設定
	_cmdList->IASetVertexBuffers(0, 1, &vertexBuffer.vertexView);
	_cmdList->IASetIndexBuffer(&indexBuffer.indexView);

	// インデックス描画
	_cmdList->DrawIndexedInstanced(indexBuffer.indexCount, 1, 0, 0, 0);
}