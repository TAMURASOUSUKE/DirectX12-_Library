#pragma once
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <filesystem>
#include <vector>
#include <stack>
#include <deque>
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ModelHandle.h"
#include "../Core/Handle/RTHandle.h"
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
	// 終了処理
	void Shutdown();
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
	TexHandle LoadTexture(const char* _filePath, bool _isData);

	// バイト列をもとに画像を持ってくる
	TexHandle LoadTextureFromMemory(const void* _data, size_t _size, bool _isData);

	// ファイル名を引数にモデルをロードする関数
	ModelHandle LoadModel(const char* _filePath);

	// 指定サイズのオフスクリーン描画先を作成する
	RTHandle CreateRenderTarget(UINT _width, UINT _height);

	// コンパイル済みPSをShader台帳へ登録する
	ShaderHandle RegisterShader(ShaderUsage _usage, ShaderStage _stage ,ComPtr<ID3DBlob> _shaderBlob);
	// 作成済みPSOをMaterial台帳へ登録する
	MaterialHandle RegisterMaterial(ShaderHandle _shader, ComPtr<ID3D12PipelineState> _pipelineState);

	// Handleをindex部分と世代部分に分ける
	TextureData* Lookup(TexHandle _handle);
	ModelData* Lookup(ModelHandle _handle);
	RenderTargetData* Lookup(RTHandle _handle);
	ShaderData* Lookup(ShaderHandle _handle);
	MaterialData* Lookup(MaterialHandle _handle);

	// ボーンのグローバルポーズを計算する
	void UpdateGlobalPose(AnimInstanceData& _instance);
	// Animation補完する関数(どのアニメーションか、ボーン、再生時刻、(出力)各ボーンの補完済みローカルポーズ)
	void SampleAnimation(const Animation& _anim, const std::vector<Bone>& _bones , float _time, std::vector<Mat4x4>& _outLocalPoses);

	// リソースを解放する
	void Unload(TexHandle _handle);
	void Unload(ModelHandle _handle);
	void Unload(RTHandle _hanlde);
	void Unload(ShaderHandle _handle);
	void Unload(MaterialHandle _handle);

	// デフォルト用の白テクスチャを取得する
	TexHandle GetDefaultTexture() const { return defaultTexture; }
	// エラー用のピンクテクスチャを取得する
	TexHandle GetErrorTexture() const { return errorTexture; }
private:
	// コンストラクタ
	GraphicsResourceManager() = default;

	// コピー禁止
	GraphicsResourceManager(const GraphicsResourceManager& _other) = delete;
	GraphicsResourceManager& operator =(const GraphicsResourceManager& _other) = delete;

	// GPUテクスチャ作成からレジストリ登録まで行うヘルパー
	TexHandle CreateTextureFromScratch(const DirectX::ScratchImage& _scratch, const DirectX::TexMetadata& _meta);

	// テクスチャの種類を受け取りuri/bufferviewを探索してロードするヘルパー
	TexHandle LoadTextureFromGltf(const cgltf_texture_view& _texView, const  std::filesystem::path& _modelDir, bool _isData);
	
	// 内部で使うメタテクスチャを作成するヘルパー(Initializeで作成用)
	TexHandle CreateMetaTexture(Vector3 _color);

	// Animation補完を助けるキーフレーム補完ヘルパー
	Vector4 SampleChannel(const AnimChannel& _ch, float _time);

private:
	ID3D12Device* device{ nullptr }; // Initializeでデバイスを受け取って保持する
	TexHandle defaultTexture; // デフォルトの白テクスチャ
	TexHandle errorTexture; // エラー用のピンクテクスチャ
	std::vector<TextureSlot> texSlots; // テクスチャリソースのスロット
	std::vector <ModelSlot> modelSlots; // モデルリソースのスロット
	std::vector<RenderTargetSlot> rtSlots; // RenderTargetのスロット
	std::vector<ShaderSlot> shaderSlots; // シェーダーのスロット
	std::vector<MaterialSlot> materialSlots; // materialのスロット
	std::stack<int> texFreeList; // テクスチャリソースのフリーリスト
	std::stack<int> modelFreeList; // モデルリソースのフリーリスト
	std::stack<int> renderTargetFreeList; // RenderTargetのフリーリスト
	std::stack<int> shaderFreeList; // Shaderのフリーリスト
	std::stack<int> materialFreeList; // materialのフリーリスト
	DeferredReleaseBatch pendingRelease{}; // まだEndFrameしていないのでFence値が決まっていない荷物
	std::deque<DeferredReleaseBatch> deferredReleases{}; // EndFrame済みでGPU完了を待っている荷物
};