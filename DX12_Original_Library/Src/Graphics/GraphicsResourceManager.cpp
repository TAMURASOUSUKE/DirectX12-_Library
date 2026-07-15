#define INITGUID
#include <algorithm>
#include <filesystem>
#include "../External/Common/d3dx12.h"
#include "../External/DirectXTex/DirectXTex.h"
#include "../External/cgltf.h"
#include"../Debug/DebugLogs.h"
#include "../Core/Handle/HandleConstant.h"
#include "../Math/TSMath.h"
#include "GraphicsDevice.h"
#include "DescriptorManager.h"
#include "GraphicsResourceManager.h"

#pragma comment(lib, "windowscodecs.lib") // WIC（LoadFromWICFile）
#pragma comment(lib, "ole32.lib")        // COM（CoInitializeEx / CoCreateInstance）


namespace {
	// cgltfから返ってくるfloat[4]やfloat[3]をvector4,3に変換するためのもの
	Vector4 ToVec4(float* _f) { return Vector4{ _f[0], _f[1], _f[2], _f[3] }; }
	Vector3 ToVec3(float* _f) { return Vector3{ _f[0], _f[1], _f[2] }; }
	// ボーンをロードするヘルパー関数(コピーコストを完全に0にする + 意図を明確にするため参照で受ける)
	void LoadBone(const cgltf_skin& _skin, const cgltf_data* _data, std::vector<Bone>& _outBones, Mat4x4& _outSkeletonRoot)
	{
		// nodeをboneに変換する
		// 変換表
		std::vector<int> nodeToBone(_data->nodes_count, -1); // ノード数分確保して-1で埋める
		for (cgltf_size b = 0; b < _skin.joints_count; b++)
		{
			// joint[b]はcgltf_node*型なのでnodeIndexに変換が必要
			cgltf_node* jointNode{ _skin.joints[b] };
			// jointNodeがdata->nodesの何番目か
			int nodeIndex{ static_cast<int>(jointNode - _data->nodes) }; // ポインタ演算でノード番号を求める
			nodeToBone[nodeIndex] = static_cast<int>(b);
		}

		// boneの親を求める
		std::vector<int> parentIndices(_skin.joints_count, -1);
		for (cgltf_size b = 0; b < _skin.joints_count; b++)
		{
			cgltf_node* boneNode{ _skin.joints[b] }; // bone[b]のnodeを取る

			// 子ノードの数だけ回す
			for (cgltf_size c = 0; c < boneNode->children_count; c++)
			{
				cgltf_node* childNode{ boneNode->children[c] }; // 子ノードのポインタを取る

				int childNodeIndex{ static_cast<int>(childNode - _data->nodes) };
				int childBone{ nodeToBone[childNodeIndex] }; // 変換表でbone番号にする

				if (childBone < 0) continue; // boneが-1ならスキップ
				parentIndices[childBone] = static_cast<int>(b); // その子の親はb
			}
		}

		_outBones.resize(_skin.joints_count); // ボーンの数だけ再確保

		for (cgltf_size b = 0; b < _skin.joints_count; b++)
		{
			cgltf_node* boneNode{ _skin.joints[b] }; // ボーンbのnodeポインタ

			// 親の代入
			_outBones[b].parentIndex = parentIndices[b];

			// localPoseの取得(matrixがある場合とTRS両対応する)
			Mat4x4 localPose{};
			if (boneNode->has_matrix)
			{
				// 行列がある場合
				Mat4x4 tmp
				{
					 Vector4{ boneNode->matrix[0],  boneNode->matrix[1],  boneNode->matrix[2],  boneNode->matrix[3]  },
					Vector4{ boneNode->matrix[4],  boneNode->matrix[5],  boneNode->matrix[6],  boneNode->matrix[7]  },
					Vector4{ boneNode->matrix[8],  boneNode->matrix[9],  boneNode->matrix[10], boneNode->matrix[11] },
					Vector4{ boneNode->matrix[12], boneNode->matrix[13], boneNode->matrix[14], boneNode->matrix[15] },
				};
				// 列優先で取得するため転置が必要
				localPose = Mat4x4::MakeTransposed(tmp);

				// hasMatrixの場合のバインドTRS分解は未対応なので今後matrix持ちモデルが来たら対応する
			}
			else
			{
				// TRS対応
				Vector3 t = ToVec3(boneNode->translation);
				Quaternion r{ boneNode->rotation[0], boneNode->rotation[1], boneNode->rotation[2], boneNode->rotation[3] };
				Vector3 s = ToVec3(boneNode->scale);

				Mat4x4 sMat{ Mat4x4::MakeScaling(s) }; // スケール行列
				Mat4x4 rMat{ r.ToMat4x4() }; // 回転行列 
				Mat4x4 tMat{ Mat4x4::MakeTranslation(t) }; // 平行移動行列
				localPose = sMat * rMat * tMat;

				// バインドポーズのTRSを保存する
				_outBones[b].bindTranslation = t;
				_outBones[b].bindRotation = r;
				_outBones[b].bindScale = s;
			}
			_outBones[b].localPose = localPose;

			// IBM
			float ibmRaw[16];
			cgltf_accessor_read_float(_skin.inverse_bind_matrices, b, ibmRaw, 16); // b番目の16要素を取り出す
			// 列優先の一時的な行列オブジェクト
			Mat4x4 ibmTmp
			{
				Vector4{ibmRaw[0], ibmRaw[1], ibmRaw[2], ibmRaw[3]},
				Vector4{ibmRaw[4], ibmRaw[5], ibmRaw[6], ibmRaw[7]},
				Vector4{ibmRaw[8], ibmRaw[9], ibmRaw[10], ibmRaw[11]},
				Vector4{ibmRaw[12], ibmRaw[13], ibmRaw[14], ibmRaw[15]}
			};

			_outBones[b].inverseBindMatrix = ibmTmp;
		}
		// Armatureを見つけて正しく計算する
		_outSkeletonRoot = Mat4x4::Identity; // デフォルト
		for (cgltf_size b = 0; b < _skin.joints_count; b++)
		{
			if (parentIndices[b] < 0) // ルートボーン
			{
				cgltf_node* armature{ _skin.joints[b]->parent }; // ルートの親 = Armature
				if (armature)
				{
					float armWorld[16];
					cgltf_node_transform_world(armature, armWorld); // ワールド変換
					// 行優先へ変換
					Mat4x4 tmp
					{
						Vector4{armWorld[0], armWorld[1], armWorld[2], armWorld[3]},
						Vector4{armWorld[4], armWorld[5], armWorld[6], armWorld[7]},
						Vector4{armWorld[8], armWorld[9], armWorld[10], armWorld[11]},
						Vector4{armWorld[12], armWorld[13], armWorld[14], armWorld[15]}
					};
					Mat4x4 zFlip{ Mat4x4::MakeScaling(Vector3{1.0f, 1.0f, -1.0f}) }; // 前後反転しないように掛ける
					_outSkeletonRoot = tmp * zFlip;
				}
				break; // ルートは一つ前提
			}
		}
	}

	// モデルをロードするときにアニメーションがあれば一緒にロードするためのヘルパー関数
	void LoadAnimation(const cgltf_data* _data, std::vector<Animation>& _outAnims)
	{
		// nodeをboneに変換する
		// 変換表
		std::vector<int> nodeToBone(_data->nodes_count, -1); // ノード数分確保して-1で埋める
		for (cgltf_size b = 0; b < _data->skins->joints_count; b++)
		{
			// joint[b]はcgltf_node*型なのでnodeIndexに変換が必要
			cgltf_node* jointNode{ _data->skins->joints[b] };
			// jointNodeがdata->nodesの何番目か
			int nodeIndex{ static_cast<int>(jointNode - _data->nodes) }; // ポインタ演算でノード番号を求める
			nodeToBone[nodeIndex] = static_cast<int>(b);
		}

		// アニメーションの数分確保
		_outAnims.resize(_data->animations_count);

		// アニメーションの数だけ回す
		for (cgltf_size i = 0; i < _data->animations_count; i++)
		{
			const cgltf_animation& anim{ _data->animations[i] }; // アニメーションを取り出す
			_outAnims[i].name = anim.name;
			_outAnims[i].channels.resize(anim.channels_count); // アニメーションのチャンネルの数を確保

			float maxTime{ 0.0f }; // 最大時間

			// チャンネル数分回す
			for (cgltf_size j = 0; j < _outAnims[i].channels.size(); j++)
			{
				const cgltf_animation_channel& ch{ anim.channels[j] }; // アニメーションのチャンネル
				// targetNodeからboneのIndexに変換
				int nodeIndex{ static_cast<int>(ch.target_node - _data->nodes) }; // ノードのインデックス
				_outAnims[i].channels[j].boneIndex = nodeToBone[nodeIndex]; // nodeをboneのインデックスに変換して埋める

				// パスの変換
				int path{};
				switch (ch.target_path)
				{
					// 位置
				case cgltf_animation_path_type_translation:
					path = AnimPath::Translation;
					break;
					// 回転
				case cgltf_animation_path_type_rotation:
					path = AnimPath::Rotation;
					break;
					// スケール
				case cgltf_animation_path_type_scale:
					path = AnimPath::Scale;
					break;
				default:
					continue; // 該当がないならスキップ
				}
				// 代入
				_outAnims[i].channels[j].path = path;

				// samplerから時刻配列と値配列を読む
				const cgltf_animation_sampler& smp{ *ch.sampler };
				int compCount{ static_cast<int>(cgltf_num_components(smp.output->type)) }; // 値を取り出す数(回転であれば4元数分必要なので)
				// キーフレームの数分回す
				cgltf_size keyCount{ smp.input->count }; // キーフレーム数
				// サイズ調整を行う
				_outAnims[i].channels[j].times.resize(keyCount);
				_outAnims[i].channels[j].values.resize(keyCount);
				for (cgltf_size k = 0; k < keyCount; k++)
				{
					// 時刻を読む
					cgltf_accessor_read_float(smp.input, k, &_outAnims[i].channels[j].times[k], 1); // 時刻はfloatで値1つ分

					// 値を読む
					float temp[4]; // 3 or 4要素のため配列で持つ
					cgltf_accessor_read_float(smp.output, k, temp, compCount); // 現在のパスに合わせて出力された値の数だけ仮の入れ物に入れる
					_outAnims[i].channels[j].values[k] = Vector4{ temp[0], temp[1], temp[2], (compCount == 4 ? temp[3] : 0) }; // Rotation意外なら最後の要素は0にする
				}

				// 全てのチャンネルを見て最大値を見る
				if (keyCount > 0)
				{
					float lastTime{ _outAnims[i].channels[j].times[keyCount - 1] }; // このチャンネルの最後の時刻
					if (lastTime > maxTime) maxTime = lastTime;
				}
			}
			// 最大時間
			_outAnims[i].duration = maxTime;
		}

	}
}

void GraphicsResourceManager::Initialize(ID3D12Device* _device)
{
	if (_device != nullptr)
	{
		device = _device;
	}

	texSlots.reserve(MAX_TEXTURE_COUNT); // 先に容量確保 + Lookupガードでタングリング防止
	modelSlots.reserve(MAX_MODEL_COUNT); // 先に容量確保 + Lookupガードでタングリング防止
	// デフォルト用の白テクスチャを作成する(初期化時に1枚だけ)
	whiteTexture = CreateWhiteTexture();
}

void GraphicsResourceManager::Shutdown()
{
	auto releaseTexture = [](TextureData& _texture)
		{
			// GPUは停止済みなのでDescriptorを即座に返す
			if (_texture.srvHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, _texture.srvHandle);
			_texture = TextureData{}; // ComPtrも含めて空にする
		};

	// まだUnloadされていないtextureを空にする
	for (TextureSlot& slot : texSlots)
	{
		releaseTexture(slot.data);
	}
	// EndFrame前にUnloadされ、Fence値が未確定のtexture
	for (TextureData& texture : pendingRelease.textures)
	{
		releaseTexture(texture);
	}
	// Fence完了待ちだったtexture
	for (DeferredReleaseBatch& batch : deferredReleases)
	{
		for (TextureData& texture : batch.textures)
		{
			releaseTexture(texture);
		}
	}
	// 生存しているmodel
	modelSlots.clear(); // ComPtrも解放
	// Unload済みで解放待ちだったmodel
	pendingRelease.models.clear();
	// DeferredReleaseBatch内のmodel
	deferredReleases.clear();
	texSlots.clear();
	pendingRelease = DeferredReleaseBatch{}; // 空にする
	// FreeListも初期状態へ戻す
	while (!texFreeList.empty())
	{
		texFreeList.pop();
	}
	while (!modelFreeList.empty())
	{
		modelFreeList.pop();
	}
	whiteTexture = TexHandle{};
	device = nullptr;
}

void GraphicsResourceManager::CommitPendingRelease(UINT64 _submittedFenceValue)
{
	// 空チェック
	if (pendingRelease.textures.empty() && pendingRelease.models.empty())
	{
		return;
	}

	// このフェンス値までGPUが進めば安全に解放できる
	pendingRelease.fenceValue = _submittedFenceValue;
	deferredReleases.push_back(std::move(pendingRelease));

	// 次のフレーム用に空にする
	pendingRelease = DeferredReleaseBatch{};
}

void GraphicsResourceManager::CollectDeferredReleases(UINT64 _completedFenceValue)
{
	// 解放待ちが入っている分だけ回す
	while (!deferredReleases.empty())
	{
		DeferredReleaseBatch& batch{ deferredReleases.front() }; // 先頭を取り出す

		// 先頭の荷物をGPUがまだ使っている
		if (batch.fenceValue > _completedFenceValue)
		{
			break;
		}

		// GPUが使い終わったのでDescriptorを返す
		for (TextureData& texture : batch.textures)
		{
			DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, texture.srvHandle);
		}

		// pop_frontによってTextureData、ModelDataが破棄されて内部のComPtrもここで解放
		deferredReleases.pop_front();
	}
}

VertexBuffer GraphicsResourceManager::CreateVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize)
{
	D3D12_HEAP_PROPERTIES heapProperties{}; // 頂点ヒープの設定
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロードヒープに設定
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // ページング

	D3D12_RESOURCE_DESC resDesc{}; // リソース設定構造体
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファとして使う
	resDesc.Width = _dataSize; // 頂点バッファのサイズ
	resDesc.Height = 1; // バッファは1D
	resDesc.DepthOrArraySize = 1; // 配列ではない
	resDesc.MipLevels = 1; // ミップマップなし
	resDesc.Format = DXGI_FORMAT_UNKNOWN; // バッファはフォーマットなし
	resDesc.SampleDesc = { 1, 0 }; // MSAAなし
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // メモリが最初から最後まで連続していることを示す

	VertexBuffer buffer{};
	HRESULT result{}; // 結果が成功しているかどうか調べるための変数
	// UploadHeap上にバッファリソースを作成する
	result = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer.resource));
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return buffer; // 失敗していたら終了

	// 頂点バッファに頂点情報をコピーする
	void* mappedData{ nullptr }; // dataを詰めるための変数
	result = buffer.resource->Map(0, nullptr, &mappedData); // バッファの仮想アドレスを取得する
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return buffer; // 失敗していたら終了

	memcpy(mappedData, _data, _dataSize); // CPUデータをGPUメモリにコピー
	buffer.resource->Unmap(0, nullptr); // 閉じる

	// 頂点バッファビューを作る
	D3D12_VERTEX_BUFFER_VIEW vertView{}; // 頂点バッファビュー
	vertView.BufferLocation = buffer.resource->GetGPUVirtualAddress(); // バッファの仮想アドレスを入れる
	vertView.SizeInBytes = _dataSize; // 全バイト数
	vertView.StrideInBytes = _strideSize; // 一つ分のバイト数

	buffer.vertexView = vertView; // GPUBufferの中に格納する
	buffer.sizeInBytes = _dataSize; // バッファ全体のサイズを入れる
	return buffer;
}

// 動的に頂点バッファを確保する
VertexBuffer GraphicsResourceManager::CreateDynamicVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize)
{
	DynamicBuffer db{ CreateDynamicBuffer(_dataSize) }; // バッファのMapと確保を行う
	if (!db.mappedPtr) return {}; // 失敗判定

	// スプライトバッチング用なのでnullガードを入れる
	if (_data != nullptr)
	{
		memcpy(db.mappedPtr, _data, _dataSize); // CPUデータをGPUメモリにコピー
	}

	VertexBuffer buffer{};
	buffer.resource = db.resource;
	buffer.mappedPtr = db.mappedPtr;

	D3D12_VERTEX_BUFFER_VIEW vertexView{}; // 頂点バッファビュー
	vertexView.StrideInBytes = _strideSize; // 一つ分のサイズ
	vertexView.BufferLocation = buffer.resource->GetGPUVirtualAddress(); // 仮想GPUアドレス
	vertexView.SizeInBytes = _dataSize; // データのサイズ

	buffer.vertexView = vertexView; // GPUBufferの中に格納する
	buffer.sizeInBytes = _dataSize; // バッファ全体のサイズを入れる
	return buffer;

}

// 確保とMapだけする
DynamicBuffer GraphicsResourceManager::CreateDynamicBuffer(UINT _dataSize)
{
	D3D12_HEAP_PROPERTIES heapProps{}; // ヒープのプロパティ設定
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロードヒープ
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // ページングなし

	D3D12_RESOURCE_DESC resDesc{}; // リソース設定構造体
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファとして使う
	resDesc.Width = _dataSize; // バッファのサイズ
	resDesc.Height = 1; // バッファは1D
	resDesc.DepthOrArraySize = 1; // 配列ではない
	resDesc.MipLevels = 1; // ミップマップなし
	resDesc.Format = DXGI_FORMAT_UNKNOWN; // バッファはフォーマットなし
	resDesc.SampleDesc = { 1, 0 }; // MSAAなし
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // メモリが最初から最後まで連続していることを示す

	DynamicBuffer buffer{}; // バッファ
	HRESULT result; // 結果判定

	// 実行中に内部の値が変わる可能性があるのでUploadHeap上に作り開いたままにしておく
	result = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer.resource));
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return buffer; // 失敗していたら終了

	// Mapする
	result = buffer.resource->Map(0, nullptr, &buffer.mappedPtr); // バッファの仮想アドレスを取得する
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return buffer; // 失敗していたら終了

	return buffer;
}

IndexBuffer GraphicsResourceManager::CreateIndexBuffer(const void* _data, UINT _dataSize, UINT _indexCount)
{
	D3D12_HEAP_PROPERTIES heapProps{}; // ヒープのプロパティ設定
	heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // アップロードヒープに設定
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // ページング

	D3D12_RESOURCE_DESC resDesc{}; // リソース設定構造体
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER; // バッファとして使う
	resDesc.Width = _dataSize; // インデックスバッファのサイズ
	resDesc.Height = 1; // バッファは1D
	resDesc.DepthOrArraySize = 1; // 配列ではない
	resDesc.MipLevels = 1; // ミップマップなし
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc = { 1, 0 }; // MSAAなし
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR; // メモリが最初から最後まで連続していることを示す

	// インデックスバッファの作成
	IndexBuffer buffer{};
	HRESULT result{};
	// 実際に作成を行うが一旦UploadHeap上に作る。今後3Dモデルを扱う際には大量のインデックスが必要なのでDefaultHeapに移し替え最適化する
	result = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer.resource));
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return buffer; // 失敗していたら終了

	// MapとUnMapを用いてインデックス情報をコピーする
	void* mappedData{ nullptr }; // Dataを詰めるための配列
	result = buffer.resource->Map(0, nullptr, &mappedData); // バッファの仮想アドレスを取得する
	DEBUG_ASSERT(SUCCEEDED(result)); // デバッグ時失敗したら場所を知らせる
	if (FAILED(result)) return buffer; // 失敗していたら終了

	memcpy(mappedData, _data, _dataSize); // CPUデータをGPUメモリにコピー
	buffer.resource->Unmap(0, nullptr); // 閉じる

	// インデックスバッファビューを作成する
	D3D12_INDEX_BUFFER_VIEW indexView{};
	indexView.BufferLocation = buffer.resource->GetGPUVirtualAddress(); // バッファの仮想アドレスを入れる
	indexView.SizeInBytes = _dataSize;
	indexView.Format = DXGI_FORMAT_R32_UINT; // インデックスなので32bitの符号なし整数

	buffer.indexView = indexView; // 設定したインデックスバッファ
	buffer.indexCount = _indexCount; // インデックスの数
	return buffer;
}

// 定数バッファの作成
ConstantBufferData GraphicsResourceManager::CreateConstantBuffer(const void* _data, UINT _dataSize)
{

	UINT alignmentedSize{ (_dataSize + 0xff) & ~0xff }; // 256の倍数に切り上げたサイズ(DX12のCBVリソースサイズが256の倍数でなければならないため)

	DynamicBuffer db{ CreateDynamicBuffer(alignmentedSize) }; // 境界用に切り上げたデータ 
	DEBUG_ASSERT(db.mappedPtr); // 失敗したら判別
	if (!db.mappedPtr) return {}; // 失敗判定

	memcpy(db.mappedPtr, _data, _dataSize); // CPUデータをGPUメモリにコピー

	// 定数バッファの作成
	ConstantBufferData buffer{};
	buffer.resource = db.resource;
	buffer.mappedPtr = db.mappedPtr;

	// 定数バッファの設定
	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc{};
	DescriptorHandle cbvHandle{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // CBVのスロットを確保

	if (!cbvHandle.IsValid())
	{
		DEBUG_LOG_ERROR("ディスクリプタヒープが枯渇しています\n");
		return ConstantBufferData{};
	}

	cbvDesc.SizeInBytes = alignmentedSize;
	cbvDesc.BufferLocation = buffer.resource->GetGPUVirtualAddress(); // バッファの仮想アドレスを取得

	// 定数バッファの作成 
	device->CreateConstantBufferView(&cbvDesc, cbvHandle.cpu);
	buffer.cbvHandle = cbvHandle;
	return buffer;
}

// 画像の読み込み
TexHandle GraphicsResourceManager::LoadTexture(const char* _filePath)
{
	DEBUG_ASSERT((texSlots.size() < MAX_TEXTURE_COUNT) && "テクスチャーロードのスロットサイズが規定値を超えました\n");
	if (texSlots.size() >= MAX_TEXTURE_COUNT) return TexHandle{};


	// DirectXTexを用いたテクスチャロード
	ID3D12Device* device{ GraphicsDevice::Instance().GetDevice() };
	HRESULT result{}; // 結果判定用

	// WICでCPUに読み込む
	std::filesystem::path path(_filePath); // std::filesystem::pathの一次オブジェクトから.c_str()をとるとタングリングするのでローカル保持する
	DirectX::TexMetadata metaData{}; // 画像のメタデータ
	DirectX::ScratchImage scratch{}; // 画像管理クラス
	result = DirectX::LoadFromWICFile(path.c_str(), DirectX::WIC_FLAGS_NONE, &metaData, scratch);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{}; // 空を返す
	}

	return CreateTextureFromScratch(scratch, metaData);
}

// バイト列から読むタイプの画像読み込み
TexHandle GraphicsResourceManager::LoadTextureFromMemory(const void* _data, size_t _size)
{
	// DirectXTexを用いたテクスチャロード
	ID3D12Device* device{ GraphicsDevice::Instance().GetDevice() };
	HRESULT result{}; // 結果判定用

	// WICでCPUに読み込む
	DirectX::TexMetadata metaData{}; // 画像のメタデータ
	DirectX::ScratchImage scratch{}; // 画像管理クラス
	result = DirectX::LoadFromWICMemory(static_cast<const uint8_t*>(_data), _size, DirectX::WIC_FLAGS_NONE, &metaData, scratch);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{}; // 空を返す
	}

	return CreateTextureFromScratch(scratch, metaData);
}

TextureData* GraphicsResourceManager::Lookup(TexHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}
	int packed{ _handle.GetRaw(PassKey{}) }; // 内部ハンドルを取り出す
	int index{ UnpackIndex(packed) }; // index取り出し
	if (index < 0 || index >= static_cast<int>(texSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	TextureSlot& slot{ texSlots[index] };
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data;
}

ModelData* GraphicsResourceManager::Lookup(ModelHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す
	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(modelSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	ModelSlot& slot{ modelSlots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data; // 実体を返す
}

ModelHandle GraphicsResourceManager::LoadModel(const char* _filePath)
{
	DEBUG_ASSERT((modelSlots.size() < MAX_MODEL_COUNT) && "モデルロードのスロットサイズが規定値を超えました\n");
	if (modelSlots.size() >= MAX_MODEL_COUNT) return ModelHandle{};

	cgltf_options options{}; // 全部0(デフォルト挙動)
	cgltf_data* data{ nullptr };

	// .glbのJSON部分を読む
	cgltf_result result{ cgltf_parse_file(&options, _filePath, &data) };
	if (result != cgltf_result_success)
	{
		// 分解失敗処理
		DEBUG_LOG_ERROR("ファイルパース失敗 path : {}\n", _filePath);
		return ModelHandle{}; // 失敗したら空を返す
	}

	// 実バイナリ(頂点/インデックスのバイト列)を展開する
	result = cgltf_load_buffers(&options, data, _filePath);
	if (result != cgltf_result_success)
	{
		DEBUG_LOG_ERROR("バッファ展開失敗\n");
		cgltf_free(data); // パース処理で確保してるので解放を行う
		return ModelHandle{}; // 失敗したら空を返す
	}

	if (data->meshes_count <= 0) return ModelHandle{}; // メッシュがなければ空を返す
	DEBUG_LOG("mesh_count : {}\n", data->meshes_count);

	ModelData modelData{}; // SubMeshを溜めるデータ
	std::filesystem::path modelDir{ std::filesystem::path(_filePath).parent_path() }; // ファイル名を除いたフォルダをとりだす。(uriの基準を出すため)
	// node配列を見て基準にループする
	for (cgltf_size i = 0; i < data->nodes_count; i++)
	{
		cgltf_node* node{ &data->nodes[i] }; // 現在のnode
		if (!node->mesh) continue; // meshを持たないnode(空ノード,ライト,カメラ等)はスキップ

		// nodeのワールド変換行列を取得する
		float nodeColMajor[16]{};
		cgltf_node_transform_world(node, nodeColMajor); // 列優先の16要素で親をたどり最終的なワールド行列を計算する(列優先 + 16要素は下で解決)

		// 列優先　-> 行優先に変更
		Mat4x4 tmp
		{
			Vector4{ nodeColMajor[0],  nodeColMajor[1],  nodeColMajor[2],  nodeColMajor[3]  },
			Vector4{ nodeColMajor[4],  nodeColMajor[5],  nodeColMajor[6],  nodeColMajor[7]  },
			Vector4{ nodeColMajor[8],  nodeColMajor[9],  nodeColMajor[10], nodeColMajor[11] },
			Vector4{ nodeColMajor[12], nodeColMajor[13], nodeColMajor[14], nodeColMajor[15] },
		};
		// 転置して正しい行優先に直す
		Mat4x4 nodeMat{ Mat4x4::MakeTransposed(tmp) };
		//　右手系から左手系に
		Mat4x4 zFlip{ Mat4x4::MakeScaling(Vector3{1.0f, 1.0f, -1.0f}) };
		Mat4x4 filnalMat{ nodeMat * zFlip };

		// このnodeがさすmeshのprimitiveを処理する
		const cgltf_mesh& mesh{ *node->mesh };
		for (cgltf_size j = 0; j < mesh.primitives_count; j++)
		{
			const cgltf_primitive& prim{ mesh.primitives[j] }; // primitive(=1サブメッシュ分)

			const cgltf_accessor* positionAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_position, 0) }; // ポジションのアクセサ
			const cgltf_accessor* normalAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_normal, 0) }; // 法線のアクセサ
			const cgltf_accessor* uvAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_texcoord, 0) }; // uvのアクセサ
			const cgltf_accessor* weightAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_weights, 0) }; // 重みのアクセサ
			const cgltf_accessor* boneAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_joints, 0) }; // ボーンのアクセサ

			// 属性が欠けているprimitiveはスキップする
			if (!positionAccessor || !normalAccessor || !uvAccessor)
			{
				DEBUG_LOG_WARNING("属性欠落のためprimitiveをスキップ node:{} primitive:{}\n", i, j);
				continue;
			}

			// 頂点数 =Position属性のアクセサのカウント
			// 属性は型で探す必要がある(順不同)
			const cgltf_size vertCount{ positionAccessor->count }; // 頂点数の取得
			const cgltf_size indexCount{ prim.indices ? prim.indices->count : 0 }; // index数の取得

			std::vector<ModelVertex> verticesData(vertCount); // このprimitive用の頂点配列
			std::vector<uint32_t> indicesData(indexCount);    // 同上インデックス

			for (cgltf_size k = 0; k < vertCount; k++)
			{
				// 生の座標を読む(ローカル座標)
				float localPos[3]{};
				cgltf_accessor_read_float(positionAccessor, k, localPos, 3);

				// スキンがない場合は焼きこみそうでない場合は焼きこまない(行なのでv * M)
				Vector4 p{ localPos[0], localPos[1], localPos[2], 1.0f };
				Vector4 worldPos;
				if (data->skins_count > 0)  // スキンモデル
				{
					// ノード変換を焼かない（ローカル座標のまま）
					worldPos = Vector4{ localPos[0], localPos[1], localPos[2], 1.0f };
				}
				else  // 静的モデル
				{
					// ノード変換を焼き込む
					Vector4 p{ localPos[0], localPos[1], localPos[2], 1.0f };
					worldPos = Mat4x4::Mul(p, filnalMat);
				}

				verticesData[k].position[0] = worldPos.x;
				verticesData[k].position[1] = worldPos.y;
				verticesData[k].position[2] = worldPos.z;

				// とりあえずnormalは今は生のまま読む
				cgltf_accessor_read_float(normalAccessor, k, verticesData[k].normal, 3);

				//uv
				cgltf_accessor_read_float(uvAccessor, k, verticesData[k].uv, 2);

				// weightがあれば読む(無ければ0番に100%)
				if (weightAccessor)
				{
					cgltf_accessor_read_float(weightAccessor, k, verticesData[k].weight, 4);
				}
				else
				{
					verticesData[k].weight[0] = 1.0f;  // ボーン0に100%（ダミー）
					verticesData[k].weight[1] = 0.0f;
					verticesData[k].weight[2] = 0.0f;
					verticesData[k].weight[3] = 0.0f;
				}

				// ボーンがあれば読む(なければ0埋め)
				if (boneAccessor)
				{
					cgltf_accessor_read_uint(boneAccessor, k, verticesData[k].bones, 4);
				}
				else
				{
					verticesData[k].bones[0] = 0;  // 全部ボーン0（ダミー）
					verticesData[k].bones[1] = 0;
					verticesData[k].bones[2] = 0;
					verticesData[k].bones[3] = 0;
				}

			}

			for (cgltf_size k = 0; k < indexCount; k++)
			{
				// indexも読む
				indicesData[k] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, k));
			}

			// 静的なGPUバッファ作成
			VertexBuffer vertBuffer{ CreateVertexBuffer(verticesData.data(), static_cast<UINT>(verticesData.size() * sizeof(ModelVertex)), sizeof(ModelVertex)) };
			IndexBuffer indexBuffer{ CreateIndexBuffer(indicesData.data(), static_cast<UINT>(indicesData.size() * sizeof(uint32_t)), static_cast<UINT>(indicesData.size())) };
			if (!vertBuffer.resource || !indexBuffer.resource)
			{
				DEBUG_LOG_WARNING("バッファ作成失敗 node:{} primitive:{}\n", i, j);
				continue;
			}

			SubMesh sub{}; // サブメッシュ
			sub.vertexBuffer = vertBuffer;
			sub.indexBuffer = indexBuffer;
			
			TexHandle baseColor{ LoadTextureFromGltf(prim.material->pbr_metallic_roughness.base_color_texture, modelDir) }; // ベースカラーテクスチャ
			TexHandle normalMap{ LoadTextureFromGltf(prim.material->normal_texture, modelDir) }; // ノーマルマップ
			TexHandle metallic{ LoadTextureFromGltf(prim.material->pbr_metallic_roughness.metallic_roughness_texture, modelDir) }; // メタリック
			TexHandle emissive{ LoadTextureFromGltf(prim.material->emissive_texture, modelDir) }; // 自己発光

			// テクスチャや各パラメータの代入
			if (prim.material)
			{
				sub.material.textures[MaterialTex::BaseColor] = baseColor;
				sub.material.textures[MaterialTex::Normal] = normalMap;
				sub.material.textures[MaterialTex::MetallicRoughness] = metallic;
				sub.material.textures[MaterialTex::Emissive] = emissive;
				sub.material.baseColorFactor = ToVec4(prim.material->pbr_metallic_roughness.base_color_factor);
				sub.material.metallic = prim.material->pbr_metallic_roughness.metallic_factor;
				sub.material.roughness = prim.material->pbr_metallic_roughness.roughness_factor;
				sub.material.emissiveFactor = ToVec3(prim.material->emissive_factor);
			}

			// BaseColorハンドルが無効なら白にする
			if (!sub.material.textures[MaterialTex::BaseColor].IsValid())
			{
				sub.material.textures[MaterialTex::BaseColor] = whiteTexture;
			}

			// 自分のテクスチャを登録していく(同じTextureを重複登録しない)
			if (baseColor.IsValid() && baseColor != whiteTexture && std::find(modelData.ownedTextures.begin(), modelData.ownedTextures.end(), baseColor) == modelData.ownedTextures.end())
			{
				// baseColor
				modelData.ownedTextures.push_back(baseColor);
			}
			if (normalMap.IsValid() && std::find(modelData.ownedTextures.begin(), modelData.ownedTextures.end(), normalMap) == modelData.ownedTextures.end())
			{
				// normalMap
				modelData.ownedTextures.push_back(normalMap);
			}
			if (metallic.IsValid() && std::find(modelData.ownedTextures.begin(), modelData.ownedTextures.end(), metallic) == modelData.ownedTextures.end())
			{
				// metallic
				modelData.ownedTextures.push_back(metallic);
			}
			if (emissive.IsValid() && std::find(modelData.ownedTextures.begin(), modelData.ownedTextures.end(), emissive) == modelData.ownedTextures.end())
			{
				// emissive
				modelData.ownedTextures.push_back(emissive);
			}

			modelData.subMeshes.push_back(sub); // 詰め込む
		}

	}

	// スキンがあればボーンを読み込む
	if (data->skins_count > 0)
	{
		LoadBone(data->skins[0], data, modelData.bones, modelData.skeletonRoot);
		DEBUG_LOG("bones loaded : {}\n", modelData.bones.size());

		// ルートボーンが1個か確認（階層の健全性チェック）
		for (size_t i = 0; i < modelData.bones.size(); i++)
		{
			if (modelData.bones[i].parentIndex < 0)
				DEBUG_LOG("root bone at index: {}\n", i);
		}

		if (data->animations_count > 0)
		{
			LoadAnimation(data, modelData.animations);
			DEBUG_LOG("animations loaded: {}\n", modelData.animations.size());
			for (const auto& a : modelData.animations)
			{
				DEBUG_LOG("anim '{}' channels : {} duration : {}\n", a.name, a.channels.size(), a.duration);
			}
		}
	}

	int index;
	// 空ではないなら再利用する
	if (!modelFreeList.empty())
	{
		index = modelFreeList.top(); // freelistから取り出す
		modelFreeList.pop(); // 削除
		modelSlots[index].data = modelData; // Unload時点で++されるので世代は据え置き
	}
	// 空なら伸ばす
	else
	{
		index = static_cast<int>(modelSlots.size());
		modelSlots.push_back({ modelData, 0 }); // 新規なので世代は0で
	}

	int packed{ Pack(index, modelSlots[index].generation) }; // パックしたハンドルを入れる
	cgltf_free(data);

	return ModelHandle(PassKey{}, packed);
}

void GraphicsResourceManager::Unload(TexHandle _handle)
{
	TextureData* data{ Lookup(_handle) };
	if (!data)
	{
		// 無効なハンドル
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return;
	}
	int index{ UnpackIndex(_handle.GetRaw(PassKey{})) }; // indexの取り出し
	// ComPtrとDescriptorHanldeを解放待ちへ移動する
	pendingRelease.textures.push_back(std::move(texSlots[index].data));
	// CPU側のハンドルは無効化する
	texSlots[index].data = TextureData{};
	texSlots[index].generation++;
	texFreeList.push(index);
}

void GraphicsResourceManager::Unload(ModelHandle _handle)
{
	ModelData* data{ Lookup(_handle) };
	if (!data)
	{
		// 無効なハンドル
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return;
	}
	int index{ UnpackIndex(_handle.GetRaw(PassKey{})) }; // indexを取り出す

	ModelData& model{ modelSlots[index].data }; // 実データ取り出し

	// このモデル自身が持っているテクスチャのみを解放する
	for (TexHandle texture : model.ownedTextures)
	{
		if (texture.IsValid() && texture != whiteTexture)
		{
			Unload(texture);
		}
	}


	pendingRelease.models.push_back(std::move(model)); // VB,IBも含めComPtrとDescriptorHanldeを解放待ちへ移動
	modelSlots[index].data = ModelData{}; // 空を入れてsubMeshごと破棄
	modelSlots[index].generation++; // 世代を増やして既存を無効化
	modelFreeList.push(index); // freelistへ返す

}

TexHandle GraphicsResourceManager::CreateTextureFromScratch(const DirectX::ScratchImage& _scratch, const DirectX::TexMetadata& _meta)
{
	HRESULT result{};

	// Defaultヒープに空のテクスチャを作る(CreateTextureは非Xbox環境の場合はCOMMONで返す。formatはmetadataのものを保持する)
	ComPtr<ID3D12Resource> texResource;
	result = DirectX::CreateTexture(device, _meta, texResource.GetAddressOf());
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{};
	}

	// UpdateSubResourceヘ渡せる形へ変換(mip/面ごとに1要素のsubresource配列)
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	result = DirectX::PrepareUpload(device, _scratch.GetImages(), _scratch.GetImageCount(), _meta, subresources);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{};
	}

	// Uploadバッファを確保する必要なバイト数はd3dx12のヘルパから
	const UINT64 uploadSize{ GetRequiredIntermediateSize(texResource.Get(), 0, static_cast<UINT>(subresources.size())) };

	ComPtr<ID3D12Resource> uploadBuffer;
	CD3DX12_HEAP_PROPERTIES uploadHeap(D3D12_HEAP_TYPE_UPLOAD);
	CD3DX12_RESOURCE_DESC bufDesc{ CD3DX12_RESOURCE_DESC::Buffer(uploadSize) };
	result = device->CreateCommittedResource(&uploadHeap, D3D12_HEAP_FLAG_NONE, &bufDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&uploadBuffer));
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{};
	}

	// アップロードを行う。recode変数にコピーとバリアを積む
	result = GraphicsDevice::Instance().ExecuteUpdate
	(
		[&](ID3D12GraphicsCommandList* _cmd)
		{
			// コピーを積む(非Xbox環境なのでCommonが来るが暗黙昇格でCOPY_DESTになる)
			UpdateSubresources(_cmd, texResource.Get(), uploadBuffer.Get(), 0, 0, static_cast<UINT>(subresources.size()), subresources.data());

			// バリアを使ってDESTからPIXEL_SHADER_RESOURCEへ遷移
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) };
			_cmd->ResourceBarrier(1, &barrier);
		}
	);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{};
	}

	// SRVの作成(メタデータから引っ張ってきたものを使う)
	DescriptorHandle srv{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // 確保
	if (!srv.IsValid())
	{
		// 枯渇していた場合の対処
		DEBUG_ASSERT(false);
		return TexHandle{};
	}

	// 設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = _meta.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // デフォルトの読み込み
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(_meta.mipLevels); // ミップレベルをメタデータから持ってくる
	device->CreateShaderResourceView(texResource.Get(), &srvDesc, srv.cpu);


	TextureData texData{}; // 戻り値用
	texData.resource = texResource;
	texData.srvHandle = srv;
	texData.width = static_cast<int>(_meta.width);
	texData.height = static_cast<int>(_meta.height);

	int index;
	// 空ではないなら再利用する
	if (!texFreeList.empty())
	{
		index = texFreeList.top(); // freelistから取り出す
		texFreeList.pop(); // 削除
		texSlots[index].data = texData; // Unload時点で++されるので世代は据え置き
	}
	// 空なら伸ばす
	else
	{
		index = static_cast<int>(texSlots.size());
		texSlots.push_back({ texData, 0 }); // 新規なので世代は0で
	}

	int packed{ Pack(index, texSlots[index].generation) }; // パックしたハンドルを入れる

	return TexHandle(PassKey{}, packed);
}

TexHandle GraphicsResourceManager::LoadTextureFromGltf(const cgltf_texture_view& _texView, const  std::filesystem::path& _modelDir)
{
	if (!_texView.texture || !_texView.texture->image) return TexHandle{}; // 空を返す
	cgltf_image* image{ _texView.texture->image };

	if (image)
	{
		if (image->uri)
		{
			std::filesystem::path texPath{ _modelDir / image->uri };  // フォルダ + ファイル名
			std::string texPathStr{ texPath.string() };
			return LoadTexture(texPathStr.c_str());
		}
		else if (image->buffer_view)
		{
			const uint8_t* bytes{ static_cast<const uint8_t*>(image->buffer_view->buffer->data) + image->buffer_view->offset };
			return LoadTextureFromMemory(bytes, image->buffer_view->size);
		}
	}

	return TexHandle{};
}

TexHandle GraphicsResourceManager::CreateWhiteTexture()
{
	DirectX::ScratchImage scratch{}; // スクラッチ
	scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1); // 1x1, 1配列, 1mip

	uint8_t white[4]{ 255, 255, 255 ,255 }; // RGBA白
	memcpy(scratch.GetPixels(), white, 4); // 生のメモリに白を書く
	return CreateTextureFromScratch(scratch, scratch.GetMetadata());
}

// globalポーズを計算する関数
void GraphicsResourceManager::UpdateGlobalPose(AnimInstanceData& _instance)
{
	ModelData* model{ GraphicsResourceManager::Instance().Lookup(_instance.handle) }; // データ部分を分解する
	if (!model) return;

	// 初回若しくはサイズが違ったときに確保しなおす
	if (_instance.globalPoses.size() != model->bones.size())
	{
		_instance.globalPoses.resize(model->bones.size()); // globalPoseのサイズ確保
		_instance.skinningMatrices.resize(model->bones.size()); // スキニング行列のサイズ確保
	}

	// 補間したlocalposeを得る
	std::vector<Mat4x4> localPose;
	if (!model->animations.empty())
	{
		const Animation& anim{ model->animations[_instance.currentAnim] }; // してのアニメーションを取り出す
		SampleAnimation(anim, model->bones, _instance.currentTime, localPose);
	}

	// ボーン数文回してglobal行列を求める
	for (size_t i = 0; i < model->bones.size(); i++)
	{
		const Bone& bone{ model->bones[i] };  // ボーンを取り出す 
		// アニメーションがあれば更新されたボーンのローカルポーズ、そうでなければバインドポーズ
		Mat4x4 local{ !model->animations.empty() ? localPose[i] : model->bones[i].localPose };

		if (bone.parentIndex < 0)
		{
			// Rootはローカルポーズがそのままグローバル行列になる(Armature変換をおこなう)
			_instance.globalPoses[i] = local * model->skeletonRoot;
		}
		else
		{
			// 自身のローカルと親との乗算を行うことで自身のglobalposeを求めることができる(親が先計算されていることが前提)
			_instance.globalPoses[i] = local * _instance.globalPoses[bone.parentIndex];
		}
	}

	// スキニング行列の計算
	for (size_t i = 0; i < model->bones.size(); i++)
	{
		// 行優先のためIBM * Globalにする
		_instance.skinningMatrices[i] = model->bones[i].inverseBindMatrix * _instance.globalPoses[i];
	}
}


void GraphicsResourceManager::SampleAnimation(const Animation& _anim, const std::vector<Bone>& _bones, float _time, std::vector<Mat4x4>& _outLocalPoses)
{

	size_t boneCount{ _bones.size() }; // ボーン数
	_outLocalPoses.resize(boneCount);
	// ボーンごとのTRSを持つキャッシュ(バインドポーズから分解した値で初期化するのでアニメーションがないボーンはバインドポーズのまま)
	// ここはホットパスなので毎フレーム再確保するのではなくメンバにして使いまわすなどの最適化を今後行う
	std::vector<Vector3> translations(boneCount); // 位置
	std::vector<Quaternion> rotations(boneCount); // 回転
	std::vector<Vector3> scales(boneCount); // スケール

	// バインドポーズのローカルポーズからTRSを取り出して初期化
	for (size_t i = 0; i < boneCount; i++)
	{
		translations[i] = _bones[i].bindTranslation;
		rotations[i] = _bones[i].bindRotation;
		scales[i] = _bones[i].bindScale;
	}

	// 全チャンネルを回してアニメーションされるボーンを上書きする
	for (const AnimChannel& ch : _anim.channels)
	{
		Vector4 v{ SampleChannel(ch, _time) }; // このチャンネルの補間値
		int bone{ ch.boneIndex };

		if (bone < 0) continue;
		switch (ch.path)
		{
		case AnimPath::Translation: translations[bone] = Vector3{ v.x, v.y, v.z }; break;
		case AnimPath::Rotation: rotations[bone] = Quaternion{ v }; break;
		case AnimPath::Scale: scales[bone] = Vector3{ v.x, v.y, v.z }; break;
		}
	}

	// TRSからlocalPosを組み立てる
	for (size_t i = 0; i < boneCount; i++)
	{
		Mat4x4 s{ Mat4x4::MakeScaling(scales[i]) };
		Mat4x4 r{ rotations[i].ToMat4x4() };
		Mat4x4 t{ Mat4x4::MakeTranslation(translations[i]) };
		_outLocalPoses[i] = s * r * t;
	}

}

Vector4 GraphicsResourceManager::SampleChannel(const AnimChannel& _ch, float _time)
{
	if (_ch.times.empty()) { return Vector4{}; } // キーフレームが0個の場合
	if (_ch.times.size() == 1) { return _ch.values[0]; } // キーフレームが1つなら補完せずにそのまま返す

	// 最初のキーフレームより前の位置の境界
	if (_time <= _ch.times.front()) { return _ch.values.front(); } // 補完せずに最初の要素を返す
	// 最後のキーフレームより後の位置の境界
	if (_time >= _ch.times.back()) { return _ch.values.back(); } // 補完せずに最後の要素を返す

	// 補完する二点間を探索する
	auto it{ std::upper_bound(_ch.times.begin(), _ch.times.end(), _time) }; // 二分探索を行い入力された時間の次に大きい要素のイテレータを取得する(O(logN))
	int index1{ static_cast<int>(it - _ch.times.begin()) }; // 後の要素のインデックス
	int index0{ index1 - 1 }; // 前の要素のインデックス

	// 補完を行う(回転はQuaternionで対応する)
	// 今の場所 / 全体で0-1の補完率を求める
	float ratio{ (_time - _ch.times[index0]) / (_ch.times[index1] - _ch.times[index0]) }; // 補完率
	if (_ch.path == AnimPath::Rotation)
	{
		// 回転であればSlerpで補完する
		Quaternion q0{ _ch.values[index0] }; // 前の値の四元数
		Quaternion q1{ _ch.values[index1] }; // 後の値の四元数
		Quaternion result{ Quaternion::Slerp(q0, q1, ratio) };  // 球面線形補完を行う
		return Vector4{ result.x, result.y, result.z, result.w };
	}
	else
	{
		// 通常の補完
		Vector4 v0{ _ch.values[index0] }; // 前の値
		Vector4 v1{ _ch.values[index1] }; // 後の値
		return v0 + (v1 - v0) * ratio; // 開始地点 + 全体 * 補完率でどのくらい進んだかを求める
	}

}
