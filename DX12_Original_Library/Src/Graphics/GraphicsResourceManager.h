#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>
#include <vector>
#include <stack>
#include <dque>
#include "../Core/Handle/TexHandle.h"
#include"../Core/Handle/ModelHandle.h"
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;
#pragma comment(lib, "d3d12.lib")

// 外部にDirectXTexが漏れるのを防ぐための前方宣言
namespace DirectX { class ScratchImage; struct TexMetadata; }
// 同様にcg_ltfが出ないようにするため
struct cgltf_texture_view;


// ShaderSystemやファサードがグラフィックリソース管理を意識せず使えるようにするクラス
class GraphicsResourceManager
{
public:
	// シングルトン化
	static GraphicsResourceManager& Instance()
	{
		static GraphicsResourceManager instance;
		return instance;
	}

	// 初期化処理
	void Initialize(ID3D12Device* _device);
	// EndFrame時にFence値を構造体へ
	void CommitPendingRelease(UINT64 _submittedFenceValue);
	// GPUが完了した時に溜まっている解放待ちを解放する処理
	void CollectDeferredReleases(UINT64 _completedFenceValue);


	// 頂点バッファの作成(Map->UnMapの固定)
	VertexBuffer CreateVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize);

	// 頂点バッファの作成(Mapしっぱなしで動的に確保を行う)
	VertexBuffer CreateDynamicVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize);

	// バッファを作るのはなく、確保とMapのみする関数
	DynamicBuffer CreateDynamicBuffer(UINT _dataSize);

	// インデックスバッファの作成
	IndexBuffer CreateIndexBuffer(const void* _data, UINT _dataSize, UINT _indexCount);

	// 定数バッファの作成
	ConstantBufferData CreateConstantBuffer(const void* _data, UINT _dataSize);

	// ファイル名を引数に画像をロードする関数
	TexHandle LoadTexture(const char* _filePath);

	// バイト列をもとに画像を持ってくる
	TexHandle LoadTextureFromMemory(const void* _data, size_t _size);

	// ファイル名を引数にモデルをロードする関数
	ModelHandle LoadModel(const char* _filePath);

	// Handleをindex部分と世代部分に分ける
	TextureData* Lookup(TexHandle _handle);
	ModelData* Lookup(ModelHandle _handle);

	// ボーンのグローバルポーズを計算する
	void UpdateGlobalPose(AnimInstanceData& _instance);
	// Animation補完する関数(どのアニメーションか、ボーン、再生時刻、(出力)各ボーンの補完済みローカルポーズ)
	void SampleAnimation(const Animation& _anim, const std::vector<Bone>& _bones , float _time, std::vector<Mat4x4>& _outLocalPoses);

	// リソースを解放する
	void Unload(TexHandle _handle);
	void Unload(ModelHandle _handle);

	// デフォルト用の白テクスチャを取得する
	TexHandle GetWhiteTexture() const { return whiteTexture; }
private:
	// コンストラクタ
	GraphicsResourceManager() = default;

	// コピー禁止
	GraphicsResourceManager(const GraphicsResourceManager& _other) = delete;
	GraphicsResourceManager& operator =(const GraphicsResourceManager& _other) = delete;

	// GPUテクスチャ作成からレジストリ登録まで行うヘルパー
	TexHandle CreateTextureFromScratch(const DirectX::ScratchImage& _scratch, const DirectX::TexMetadata& _meta);

	// テクスチャの種類を受け取りuri/bufferviewを探索してロードするヘルパー
	TexHandle LoadTextureFromGltf(const cgltf_texture_view& _texView, const  std::filesystem::path& _modelDir);
	
	// デフォルト用の白色のテクスチャを作成するヘルパー(Initializeで作成用)
	TexHandle CreateWhiteTexture();

	// Animation補完を助けるキーフレーム補完ヘルパー
	Vector4 SampleChannel(const AnimChannel& _ch, float _time);

private:
	ID3D12Device* device{ nullptr }; // Initializeでデバイスを受け取って保持する
	TexHandle whiteTexture; // デフォルトの白テクスチャ
	std::vector<TextureSlot> texSlots; // テクスチャリソースのスロット
	std::vector <ModelSlot> modelSlots; // モデルリソースのスロット
	std::stack<int> texFreeList; // テクスチャリソースのフリーリスト
	std::stack<int> modelFreeList; // モデルリソースのフリーリスト
	DeferredReleaseBatch pendingRelease{}; // まだEndFrameしていないのでFence値が決まっていない荷物
	std::deque<DeferredReleaseBatch> deferredReleases{}; // EndFrame済みでGPU完了を待っている荷物
};