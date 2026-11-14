#define INITGUID
#include <algorithm>
#include <filesystem>
#include <cstring>
#include <span>
#include <unordered_map>
#include <utility>
#include <limits>
#include "../External/Common/d3dx12.h"
#include "../External/DirectXTex/DirectXTex.h"
#include "../External/cgltf.h"
#include"../Debug/DebugLogs.h"
#include "../Core/Handle/HandlePacking.h"
#include "../Math/TSMath.h"
#include "UploadType.h"
#include "GraphicsDevice.h"
#include "DescriptorManager.h"
#include "GraphicsResourceManager.h"

#pragma comment(lib, "windowscodecs.lib") // WIC（LoadFromWICFile）
#pragma comment(lib, "ole32.lib")        // COM（CoInitializeEx / CoCreateInstance）


namespace {

	Vector4 ToVec4(float* _f) { return Vector4{ _f[0], _f[1], _f[2], _f[3] }; }
	Vector3 ToVec3(float* _f) { return Vector3{ _f[0], _f[1], _f[2] }; }

	// 溜まっている記録を送信してGPU待機を行いBatchの回収を行う
	bool FlushUploads(UploadSystem& _upload)
	{
		bool isRecorded{ false };
		const auto& recording{
			[&](ID3D12GraphicsCommandList* _cmd) { isRecorded = _upload.RecordPendingUploads(_cmd); }
		};

		// 記録して待機する
		HRESULT result{ GraphicsDevice::Instance().ExecuteImmediate(recording) };
		if (FAILED(result))
		{
			DEBUG_LOG_ERROR("記録の実行命令に失敗しました\n");
			return false;
		}
		if (!isRecorded)
		{
			DEBUG_LOG_ERROR("予約されたコピーと状態遷移の記録に失敗しました\n");
			return false;
		}

		// 記録したBatchがあるなら実行してフェンス値を紐づけ
		if (_upload.HasRecordedUploads())
		{
			if (!_upload.CommitSubmittedUploads(GraphicsDevice::Instance().GetLastSubmittedFenceValue()))
			{
				DEBUG_LOG_ERROR("Fence値の関連付けに失敗しました\n");
				return false;
			}
		}

		// Batchの回収を行う
		_upload.CollectCompletedUploads(GraphicsDevice::Instance().GetCompletedFenceValue());
		return true;
	}

	// CPUデータをDefaultヒープへ転送する
	ComPtr<ID3D12Resource> CreateStaticBufferResource(ID3D12Device* _device, const void* _data, UINT _dataSize, D3D12_RESOURCE_STATES _finalState, UploadSystem& _upload)
	{
		if (!_device || !_data || _dataSize == 0)
		{
			DEBUG_LOG_ERROR("静的バッファ作成に不正な引数が渡されました\n");
			return nullptr;
		}

		const CD3DX12_RESOURCE_DESC bufferDesc{ CD3DX12_RESOURCE_DESC::Buffer(_dataSize) };
		// 最終的に頂点やIndexを置くGPU用領域
		ComPtr<ID3D12Resource> defaultBuffer{};
		const CD3DX12_HEAP_PROPERTIES defaultHeap{ D3D12_HEAP_TYPE_DEFAULT };

		// バッファの初期状態はCOMMON(Textureリソース等ではないためレイアウトがないから)
		HRESULT result{ _device->CreateCommittedResource(&defaultHeap, D3D12_HEAP_FLAG_NONE, &bufferDesc, D3D12_RESOURCE_STATE_COMMON, nullptr, IID_PPV_ARGS(&defaultBuffer)) };
		DEBUG_ASSERT(SUCCEEDED(result));
		if (FAILED(result))
		{
			DEBUG_LOG_ERROR("Defaultヒープの作成に失敗しました\n");
			return nullptr;
		}

		UploadResult uploadResult{ _upload.EnqueueBuffer(defaultBuffer.Get(), _data, _dataSize, D3D12_RESOURCE_STATE_COMMON, _finalState) };
		switch (uploadResult)
		{
		case UploadResult::Success:
			break;
		case UploadResult::OverBudget:
			// 現状溜まっている記録を送信して再利用できる状態にする
			if (!FlushUploads(_upload))
			{
				DEBUG_LOG_ERROR("溜まっている記録の送信に失敗しました\n");
				return nullptr;
			}
			// もう一度転送予約
			uploadResult = _upload.EnqueueBuffer(defaultBuffer.Get(), _data, _dataSize, D3D12_RESOURCE_STATE_COMMON, _finalState);
			if (uploadResult != UploadResult::Success)
			{
				DEBUG_LOG_ERROR("再度試しましたがBuffer転送予約に失敗しました\n");
				return nullptr;
			}
			break;
		case UploadResult::OtherFailed:
			DEBUG_LOG_ERROR("Buffer転送予約に失敗しました\n");
			return nullptr;
		}

		return defaultBuffer;
	}

	// cglTFのfloat[16]から行優先でMat4x4を作る補助関数
	Mat4x4 MakeMat4x4FromCglTFFloat(const cgltf_float* _matData)
	{
		if (!_matData)
		{
			DEBUG_LOG_ERROR("渡されたcglTFのfloatが不正です\n");
			return Mat4x4{ Mat4x4::Identity };
		}
		return Mat4x4
		{
			Vector4{_matData[0], _matData[1], _matData[2], _matData[3]},
			Vector4{_matData[4], _matData[5], _matData[6], _matData[7]},
			Vector4{_matData[8], _matData[9], _matData[10], _matData[11]},
			Vector4{_matData[12], _matData[13], _matData[14], _matData[15]},
		};
	}

	// 親をたどって見つけたBoneとそこまでの変換
	struct AncestorBone
	{
		int boneIndex{ -1 }; // 見つからなければ-1
		Mat4x4 localToBone{ Mat4x4::Identity }; // 頂点をBoneの空間へ移す行列
	};

	// 親をたどってボーンを探す関数
	AncestorBone FindAncestorBone(const cgltf_node* _node, const cgltf_data* _data, std::span<const int> _nodeToBone)
	{
		if (!_node)
		{
			DEBUG_LOG_ERROR("渡されたnodeが不正です\n");
			return AncestorBone{};
		}

		if (!_data)
		{
			DEBUG_LOG_ERROR("渡されたdataが不正です\n");
			return AncestorBone{};
		}

		if (_nodeToBone.empty())
		{
			DEBUG_LOG_ERROR("渡された変換表が不正です\n");
			return AncestorBone{};
		}

		Mat4x4 localToBone{ Mat4x4::Identity }; // たどったnodeの変換を溜める
		for (const cgltf_node* current{ _node }; current; current = current->parent)
		{
			// currentがnodesの何番目かをポインタ演算で求めて表を引く
			const std::size_t nodeIndex{ static_cast<std::size_t>(current - _data->nodes) };
			const int boneIndex{ _nodeToBone[nodeIndex] };
			if (boneIndex >= 0) return AncestorBone{ boneIndex, localToBone };

			// Boneではないnodeは自分の変換を掛けて親の空間へ上がる
			cgltf_float local[16]{};
			cgltf_node_transform_local(current, local);
			localToBone = localToBone * MakeMat4x4FromCglTFFloat(local);
		}
		return AncestorBone{}; // Boneの下にいないmesh
	}

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
			_outBones[b].name = boneNode->name ? boneNode->name : std::string{};

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
			// 行優先の一時的な行列オブジェクト
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

bool GraphicsResourceManager::Setup(ID3D12Device* _device)
{
	if (!_device)
	{
		DEBUG_LOG_ERROR("不正なデバイスが渡されました\n");
		return false;
	}

	if (device)
	{
		DEBUG_LOG_ERROR("すでにGraphicsResourceManagerは初期化されています\n");
		return false;
	}

	// tryを使って確保に失敗したらエラーログを出すようにする
	try
	{
		// 先に容量確保 + ロード時ガードでダングリング防止
		texSlots.reserve(MAX_TEXTURE_COUNT);
		modelSlots.reserve(MAX_MODEL_COUNT);
		rtSlots.reserve(MAX_RENDER_TARGET_COUNT);
		shaderSlots.reserve(MAX_CUSTOM_SHADER_COUNT);
		materialSlots.reserve(MAX_MATERIAL_COUNT);
		computePipelineSlots.reserve(MAX_COMPUTE_PIPELINE_COUNT);
		gpuBufferSlots.reserve(MAX_GPU_BUFFER_COUNT);
	}
	catch (const std::bad_alloc&)
	{
		DEBUG_LOG_ERROR("GraphicsResourceManagerの管理配列のメモリ確保に失敗しました\n");
		return false;
	}

	// 転送準備
	if (!uploadSystem.Setup(_device, DEFAULT_UPLOAD_PAGE_SIZE, MAX_UPLOAD_POOL_BYTES))
	{
		DEBUG_LOG_ERROR("UploadSystemの初期化に失敗しました\n");
		return false;
	}

	device = _device;

	// デフォルト用の白テクスチャを作成する(初期化時に1枚だけ)
	defaultTexture = CreateMetaTexture({ 1.0f, 1.0f, 1.0f });
	if (!defaultTexture.IsValid())
	{
		DEBUG_LOG_ERROR("デフォルトのテクスチャ生成に失敗しました\n");
		Shutdown();
		return false;
	}
	// エラー用のピンクテクスチャを作成する
	errorTexture = CreateMetaTexture({ 1.0f, 0.0f, 1.0f });
	if (!errorTexture.IsValid())
	{
		DEBUG_LOG_ERROR("エラーテクスチャ生成に失敗しました\n");
		Shutdown();
		return false;
	}

	// nullSRVの作成(何もリソースが紐づけされていないディスクリプタ)
	nullTexSRV = DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV); // ヒープを確保
	if (!nullTexSRV.IsValid())
	{
		DEBUG_LOG_ERROR("ユーザー未入力用のTextureSlot用SRVの確保に失敗しました\n");
		Shutdown(); 
		return false;
	}
	nullBufferSRV = DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV); // ヒープを確保
	if (!nullBufferSRV.IsValid())
	{
		DEBUG_LOG_ERROR("ユーザー未入力用のBufferSlot用SRVの確保に失敗しました\n");
		Shutdown();
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC nullTexSRVDesc{};
	nullTexSRVDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM; // 線形
	nullTexSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // Shader側でtexture2Dで宣言するので食い違いがないようにする
	nullTexSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 入れられた色通りにする
	nullTexSRVDesc.Texture2D.MipLevels = 1; // nullなので1枚だけの設定
	_device->CreateShaderResourceView(nullptr, &nullTexSRVDesc, nullTexSRV.cpu); // リソースと紐づけしないのでnull
	D3D12_SHADER_RESOURCE_VIEW_DESC nullBufferSRVDesc{};
	nullBufferSRVDesc.Format = DXGI_FORMAT_UNKNOWN;
	nullBufferSRVDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	nullBufferSRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 入れられた色通りにする
	nullBufferSRVDesc.Buffer.NumElements = 1;
	nullBufferSRVDesc.Buffer.StructureByteStride = 4;
	_device->CreateShaderResourceView(nullptr, &nullBufferSRVDesc, nullBufferSRV.cpu); // リソースと紐づけしないのでnull
	return true;
}

void GraphicsResourceManager::Shutdown()
{
	uploadSystem.Shutdown();

	// テクスチャ削除ラムダ
	const auto releaseTexture
	{
		[](TextureData& _texture)
		{
			// GPUは停止済みなのでDescriptorを即座に返す
			if (_texture.srvHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, _texture.srvHandle);
			if (_texture.uavHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, _texture.uavHandle);
			_texture = TextureData{}; // ComPtrも含めて空にする
		}
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


	// RT削除ラムダ
	const auto releaseRenderTarget
	{
		[](RenderTargetData& _rt)
		{
			// RTVとSRVを解放する
			if (_rt.rtvHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::RTV, _rt.rtvHandle);
			if (_rt.srvHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, _rt.srvHandle);
			_rt = RenderTargetData{}; // 初期状態へ戻す
		}
	};

	// まだUnloadされていないRT
	for (RenderTargetSlot& slot : rtSlots)
	{
		releaseRenderTarget(slot.data);
	}
	// EndFrame前にUnloadされたRT
	for (RenderTargetData& rt : pendingRelease.renderTargets)
	{
		releaseRenderTarget(rt);
	}
	// Fence待ちのRT
	for (DeferredReleaseBatch& batch : deferredReleases)
	{
		for (RenderTargetData& rt : batch.renderTargets)
		{
			releaseRenderTarget(rt);
		}
	}
	rtSlots.clear(); // 空にする

	// GPUの待機完了後にBuffer本体とDescriptorを解放する
	const auto releaseGPUBuffer
	{
		[](GPUBufferData& _buffer)
		{
			if (_buffer.srvHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, _buffer.srvHandle);
			if (_buffer.uavHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, _buffer.uavHandle);
			_buffer = GPUBufferData{};
		}
	};

	// まだUnloadされていないBuffer
	for (GPUBufferSlot& slot : gpuBufferSlots)
	{
		releaseGPUBuffer(slot.data);
	}

	// Unload済みでFenceが未確定のBuffer
	for (GPUBufferData& buffer : pendingRelease.gpuBuffers)
	{
		releaseGPUBuffer(buffer);
	}

	// Fence完了待ちだったBuffer
	for (DeferredReleaseBatch& batch : deferredReleases)
	{
		for (GPUBufferData& buffer : batch.gpuBuffers)
		{
			releaseGPUBuffer(buffer);
		}
	}
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, nullTexSRV);
	nullTexSRV = {};
	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, nullBufferSRV);
	nullBufferSRV = {};
	// 生存しているmodel
	modelSlots.clear(); // ComPtrも解放
	// Unload済みで解放待ちだったmodel
	pendingRelease.models.clear();
	// DeferredReleaseBatch内のmodel
	deferredReleases.clear();
	texSlots.clear();
	materialSlots.clear();
	shaderSlots.clear();
	computePipelineSlots.clear();
	gpuBufferSlots.clear();
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
	while (!renderTargetFreeList.empty())
	{
		renderTargetFreeList.pop();
	}
	while (!shaderFreeList.empty())
	{
		shaderFreeList.pop();
	}
	while (!materialFreeList.empty())
	{
		materialFreeList.pop();
	}
	while (!computePipelineFreeList.empty())
	{
		computePipelineFreeList.pop();
	}
	while (!gpuBufferFreeList.empty())
	{
		gpuBufferFreeList.pop();
	}
	defaultTexture = TexHandle{};
	errorTexture = TexHandle{};
	device = nullptr;
}

void GraphicsResourceManager::CommitPendingRelease(UINT64 _submittedFenceValue)
{
	// 空チェック
	if (pendingRelease.textures.empty() && pendingRelease.models.empty() &&
		pendingRelease.renderTargets.empty() && pendingRelease.pipelineStates.empty() &&
		pendingRelease.computePipelines.empty() && pendingRelease.gpuBuffers.empty())
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
		if (batch.fenceValue > _completedFenceValue) break;

		// GPUが使い終わったのでDescriptorを返す
		for (TextureData& texture : batch.textures)
		{
			DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, texture.srvHandle);
			if (texture.uavHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, texture.uavHandle); // ファイルからの読み込みにはuavはないので確認してから
		}

		for (RenderTargetData& rt : batch.renderTargets)
		{
			// 両方ともここで開放
			DescriptorManager::Instance().Free(HeapType::RTV, rt.rtvHandle);
			DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, rt.srvHandle);
		}

		// GPUが使用し終えたBufferのDescriptorを返す
		for (GPUBufferData& buffer : batch.gpuBuffers)
		{
			if (buffer.srvHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, buffer.srvHandle);
			if (buffer.uavHandle.IsValid()) DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, buffer.uavHandle);
		}

		// pop_frontによって各リソースが破棄されて内部のComPtrもここで解放
		deferredReleases.pop_front();
	}
}

VertexBuffer GraphicsResourceManager::CreateVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize)
{
	VertexBuffer buffer{};
	if (_strideSize == 0)
	{
		DEBUG_LOG_ERROR("頂点バッファのstrideが0です\n");
		return buffer;
	}
	// バッファ全体のサイズが頂点一つ分のサイズで割り切れるか確認
	if (_dataSize % _strideSize != 0)
	{
		DEBUG_LOG_ERROR("頂点バッファサイズがstrideで割り切れません : size = {} stride = {}\n", _dataSize, _strideSize);
		return buffer;
	}

	// DEFAULTヒープへ送る
	buffer.resource = CreateStaticBufferResource(device, _data, _dataSize, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER, uploadSystem);
	if (!buffer.resource)
	{
		return buffer;
	}
	// DEFAULTヒープ上のリソースを頂点バッファとして読む
	buffer.vertexView.BufferLocation = buffer.resource->GetGPUVirtualAddress(); // GPUBufferの中に格納する
	buffer.vertexView.StrideInBytes = _strideSize;
	buffer.vertexView.SizeInBytes = _dataSize;
	buffer.sizeInBytes = _dataSize; // バッファ全体のサイズを入れる
	// 静的デフォルトHeapなのでmappedPtrは持たない
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
	IndexBuffer buffer{};
	if (_indexCount == 0)
	{
		DEBUG_LOG_ERROR("Index数が0です\n");
		return buffer;
	}

	// 現在のIBはR32_UINT固定なので1index4byte
	const UINT64 expectedSize{ static_cast<UINT64>(_indexCount) * sizeof(uint32_t) };
	if (expectedSize != _dataSize)
	{
		DEBUG_LOG_ERROR("Index数とバッファサイズが一致しません : count = {} size = {}\n", _indexCount, _dataSize);
		return buffer;
	}

	// Defaultヒープへ送る
	buffer.resource = CreateStaticBufferResource(device, _data, _dataSize, D3D12_RESOURCE_STATE_INDEX_BUFFER, uploadSystem);
	if (!buffer.resource) return buffer;

	// DEFAULTヒープ上のリソースをインデックスバッファとして読む
	buffer.indexView.BufferLocation = buffer.resource->GetGPUVirtualAddress(); // バッファの仮想アドレスを入れる
	buffer.indexView.SizeInBytes = _dataSize;
	buffer.indexView.Format = DXGI_FORMAT_R32_UINT; // インデックスなので32bitの符号なし整数
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
TexHandle GraphicsResourceManager::LoadTexture(const char* _filePath, bool _isData)
{

	// DirectXTexを用いたテクスチャロード
	ID3D12Device* device{ GraphicsDevice::Instance().GetDevice() };
	HRESULT result{}; // 結果判定用
	// データ画像かデフラグを分ける
	DirectX::WIC_FLAGS flag{ _isData ? DirectX::WIC_FLAGS_IGNORE_SRGB : DirectX::WIC_FLAGS_DEFAULT_SRGB };
	// WICでCPUに読み込む
	std::filesystem::path path(_filePath); // std::filesystem::pathの一次オブジェクトから.c_str()をとるとタングリングするのでローカル保持する
	DirectX::TexMetadata metaData{}; // 画像のメタデータ
	DirectX::ScratchImage scratch{}; // 画像管理クラス
	result = DirectX::LoadFromWICFile(path.c_str(), flag, &metaData, scratch);
	DEBUG_ASSERT(SUCCEEDED(result) && "LoadFromWICFileに失敗しました");
	if (FAILED(result))
	{
		return TexHandle{}; // 空を返す
	}

	// Facade側で指定された用とに合わせてScratchImage本体と各Imageのfomatをそろえて変更する
	const DXGI_FORMAT desiredFormat{ _isData ? DirectX::MakeLinear(metaData.format) : DirectX::MakeSRGB(metaData.format) };
	if (!scratch.OverrideFormat(desiredFormat))
	{
		DEBUG_LOG_ERROR("テクスチャのFormat変更に失敗しました Path={}\n", _filePath);
		return TexHandle{};
	}

	// オーバーライド後のメタデータを取得しなおす
	metaData = scratch.GetMetadata();

	return CreateTextureFromScratch(scratch, metaData);
}

// バイト列から読むタイプの画像読み込み
TexHandle GraphicsResourceManager::LoadTextureFromMemory(const void* _data, size_t _size, bool _isData)
{
	// DirectXTexを用いたテクスチャロード
	ID3D12Device* device{ GraphicsDevice::Instance().GetDevice() };
	HRESULT result{}; // 結果判定用
	// データ画像かデフラグを分ける
	DirectX::WIC_FLAGS flag{ _isData ? DirectX::WIC_FLAGS_IGNORE_SRGB : DirectX::WIC_FLAGS_DEFAULT_SRGB };

	// WICでCPUに読み込む
	DirectX::TexMetadata metaData{}; // 画像のメタデータ
	DirectX::ScratchImage scratch{}; // 画像管理クラス
	result = DirectX::LoadFromWICMemory(static_cast<const uint8_t*>(_data), _size, flag, &metaData, scratch);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{}; // 空を返す
	}

	// Facade側で指定された用とに合わせてScratchImage本体と各Imageのfomatをそろえて変更する
	const DXGI_FORMAT desiredFormat{ _isData ? DirectX::MakeLinear(metaData.format) : DirectX::MakeSRGB(metaData.format) };
	if (!scratch.OverrideFormat(desiredFormat))
	{
		DEBUG_LOG_ERROR("テクスチャのFormat変更に失敗しました\n", );
		return TexHandle{};
	}

	// オーバーライド後のメタデータを取得しなおす
	metaData = scratch.GetMetadata();

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

RenderTargetData* GraphicsResourceManager::Lookup(RTHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す
	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(rtSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	RenderTargetSlot& slot{ rtSlots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data; // 実体を返す
}

ShaderData* GraphicsResourceManager::Lookup(ShaderHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す
	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(shaderSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	ShaderSlot& slot{ shaderSlots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data; // 実体を返す
}
MaterialData* GraphicsResourceManager::Lookup(MaterialHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す
	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(materialSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	MaterialSlot& slot{ materialSlots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}
	return &slot.data; // 実体を返す
}

ComputePipelineData* GraphicsResourceManager::Lookup(ComputePipelineHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す

	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(computePipelineSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	ComputePipelineSlot& slot{ computePipelineSlots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}

	// 実行に必要なRootSigとPSOが存在するか確認
	if (!slot.data.pipelineState || !slot.data.rootSignature)
	{
		DEBUG_LOG_ERROR("ComputePipelineの実データが無効です\n");
		return nullptr;
	}

	return &slot.data; // 実体を返す
}

GPUBufferData* GraphicsResourceManager::Lookup(GPUBufferHandle _handle)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return nullptr; // 無効なハンドルならnull
	}

	int packed{ _handle.GetRaw(PassKey{}) }; // 生の値(内部ハンドルを取得)
	int index{ UnpackIndex(packed) }; // index部分を取り出す

	// 範囲外チェック
	if (index < 0 || index >= static_cast<int>(gpuBufferSlots.size()))
	{
		DEBUG_LOG_ERROR("ハンドルに範囲外のサイズが渡されました\n");
		return nullptr; // 範囲チェック
	}
	GPUBufferSlot& slot{ gpuBufferSlots[index] }; // スロットの指定ハンドル部分を取り出す
	// 世代チェック
	if (UnpackGen(packed) != static_cast<int>(slot.generation))
	{
		DEBUG_LOG_WARNING("世代が異なります\n");
		return nullptr; // 世代チェック
	}

	// バッファ本体が存在するか確認
	if (!slot.data.resource)
	{
		DEBUG_LOG_ERROR("GPUBufferの実データが無効です\n");
		return nullptr;
	}

	return &slot.data; // 実体を返す
}

ModelHandle GraphicsResourceManager::LoadModel(const char* _filePath)
{
	// 新規スロットを追加できる、または解放済みスロットを再利用できるか
	const bool canRegister{ !modelFreeList.empty() || modelSlots.size() < MAX_MODEL_COUNT };

	DEBUG_ASSERT(canRegister && "モデルの登録上限に達しました\n");

	if (!canRegister)
	{
		DEBUG_LOG_ERROR(
			"モデルの登録上限に達しました\n"
		);
		return ModelHandle{};
	}


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

	if (data->meshes_count <= 0)
	{
		DEBUG_LOG_WARNING("モデル内にメッシュがありません\n");
		cgltf_free(data);
		return ModelHandle{}; // メッシュがなければ空を返す
	}

	ModelData modelData{}; // SubMeshを溜めるデータ
	std::filesystem::path modelDir{ std::filesystem::path(_filePath).parent_path() }; // ファイル名を除いたフォルダをとりだす。(uriの基準を出すため)

	// glTF内で同じ画像が複数のマテリアルやprimitiveから参照されたら同じGPUテクスチャを再利用する
	// ColorとDataではsRGBの解釈が異なるので二つ
	std::unordered_map<const cgltf_image*, TexHandle> colorTextureCache{};
	std::unordered_map<const cgltf_image*, TexHandle> dataTextureCache{};

	// 同自画像を探索してからロードするラムダ
	auto loadGltfTextureCached =
		[&](const cgltf_texture_view& _textureView, bool _isData)->TexHandle
		{
			// テクスチャまたは画像が設定されていなければ空ハンドル
			if (!_textureView.texture || !_textureView.texture->image) return TexHandle{};

			const cgltf_image* image{ _textureView.texture->image };

			// フラグによって分ける
			auto& cache{ _isData ? dataTextureCache : colorTextureCache };

			// すでにロード済みなら作成済みのHandleを再利用
			const auto found{ cache.find(image) };
			if (found != cache.end()) return found->second;

			// この画像が初めて出たときだけGPUリソースの作成を行う
			const TexHandle loadedTexture{ LoadTextureFromGltf(_textureView, modelDir, _isData) };
			// 無効ハンドルも記録し、壊れた同一画像に対して何度もロードを行わないようにする
			cache.emplace(image, loadedTexture);
			return loadedTexture;
		};

	std::vector<int> nodeToBone(data->nodes_count, -1); // ノード数分確保して-1で埋める
	// 変換表 スキンがあるときだけ変換する
	if (data->skins && data->skins_count > 0)
	{
		for (cgltf_size b = 0; b < data->skins->joints_count; b++)
		{
			// joint[b]はcgltf_node*型なのでnodeIndexに変換が必要
			cgltf_node* jointNode{ data->skins[0].joints[b] };
			// jointNodeがdata->nodesの何番目か
			int nodeIndex{ static_cast<int>(jointNode - data->nodes) }; // ポインタ演算でノード番号を求める
			nodeToBone[nodeIndex] = static_cast<int>(b);
		}
	}

	// node配列を見て基準にループする
	for (cgltf_size i = 0; i < data->nodes_count; i++)
	{
		cgltf_node* node{ &data->nodes[i] }; // 現在のnode
		if (!node->mesh) continue; // meshを持たないnode(空ノード,ライト,カメラ等)はスキップ

		// スキンモデルの中のスキンのないモデルは親をたどってボーンへ付ける
		AncestorBone ancestorBone{};
		Mat4x4 attachMatrix{ Mat4x4::Identity }; // 頂点をBoneのバインド時の空間へ移す行列
		if (data->skins_count > 0)
		{
			ancestorBone = FindAncestorBone(node, data, nodeToBone);
			if (ancestorBone.boneIndex >= 0)
			{
				Mat4x4 inverseBindMatrix{ Mat4x4::Identity }; // IBMが無いglTFは単位行列として扱う
				if (data->skins[0].inverse_bind_matrices)
				{
					cgltf_float ibmRaw[16]{};
					cgltf_accessor_read_float(data->skins[0].inverse_bind_matrices, static_cast<cgltf_size>(ancestorBone.boneIndex), ibmRaw, 16);
					inverseBindMatrix = MakeMat4x4FromCglTFFloat(ibmRaw);
				}
				// 頂点 * localToBone でBoneの空間 さらにIBMの逆を掛けてスキンのmeshと同じ空間へ
				attachMatrix = ancestorBone.localToBone * inverseBindMatrix.MakeAffineInverse();
			}
		}

		// nodeのワールド変換行列を取得する
		float nodeColMajor[16]{};
		cgltf_node_transform_world(node, nodeColMajor); // 列優先の16要素で親をたどり最終的なワールド行列を計算する(列優先 + 16要素は下で解決)

		// 列優先　-> 行優先に変更
		Mat4x4 nodeMat{ MakeMat4x4FromCglTFFloat(nodeColMajor) };
		//　右手系から左手系に
		Mat4x4 zFlip{ Mat4x4::MakeScaling(Vector3{1.0f, 1.0f, -1.0f}) };
		Mat4x4 finalMat{ nodeMat * zFlip };

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
			const bool isAttachedToBone{ data->skins_count > 0 && !boneAccessor && ancestorBone.boneIndex >= 0 }; // スキンモデルの中でjointの属性を持たず親にBoneがいるmeshか

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


				if (isAttachedToBone) // Boneへ付けるmesh
				{
					// 付けるBoneのバインド時の空間へ移す
					worldPos = Mat4x4::Mul(p, attachMatrix);
				}
				else if (data->skins_count > 0)  // スキンモデル
				{
					// ノード変換を焼かない（ローカル座標のまま）
					worldPos = Vector4{ localPos[0], localPos[1], localPos[2], 1.0f };
				}
				else  // 静的モデル
				{
					// ノード変換を焼き込む
					Vector4 p{ localPos[0], localPos[1], localPos[2], 1.0f };
					worldPos = Mat4x4::Mul(p, finalMat);
				}

				verticesData[k].position[0] = worldPos.x;
				verticesData[k].position[1] = worldPos.y;
				verticesData[k].position[2] = worldPos.z;

				// とりあえずnormalは今は生のまま読む
				cgltf_accessor_read_float(normalAccessor, k, verticesData[k].normal, 3);

				// Boneへ付けるmeshは位置と同じ変換で法線の向きも移す
				if (isAttachedToBone)
				{
					Vector3 normal{ attachMatrix.TransformDirection(Vector3{ verticesData[k].normal[0], verticesData[k].normal[1], verticesData[k].normal[2] }) };
					normal.Normalize();
					verticesData[k].normal[0] = normal.x;
					verticesData[k].normal[1] = normal.y;
					verticesData[k].normal[2] = normal.z;
				}

				// uv
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
					verticesData[k].bones[0] = isAttachedToBone ? static_cast<uint32_t>(ancestorBone.boneIndex) : 0;  // 付けるBone 無ければボーン0（ダミー）
					verticesData[k].bones[1] = 0;
					verticesData[k].bones[2] = 0;
					verticesData[k].bones[3] = 0;
				}

			}

			// Triangleか
			if (prim.type != cgltf_primitive_type_triangles)
			{
				DEBUG_LOG_WARNING("Triangle以外のPrimitiveは現在対応していません\n");
				continue;
			}
			// 三角形情報の確認
			if (!prim.indices || indexCount % 3 != 0)
			{
				DEBUG_LOG_WARNING("モデルのIndex情報が三角形として不正です\n");
				continue;
			}

			for (cgltf_size k = 0; k < indexCount; k += 3)
			{
				// indexも読む
				indicesData[k] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, k));

				// 右手系->左手系に合わせてインデックス順を逆にするため2番目を3番に3番目を2番目にする
				indicesData[k + 1] = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, k + 2));
				indicesData[k + 2] = static_cast<std::uint32_t>(cgltf_accessor_read_index(prim.indices, k + 1));
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

			TexHandle baseColor{}; // ベースカラーテクスチャ
			TexHandle normalMap{}; // ノーマルマップ
			TexHandle metallic{}; // メタリック
			TexHandle emissive{}; // 自己発光

			// テクスチャや各パラメータの代入
			if (prim.material)
			{
				baseColor = loadGltfTextureCached(prim.material->pbr_metallic_roughness.base_color_texture, false); // ベースカラーテクスチャ
				normalMap = loadGltfTextureCached(prim.material->normal_texture, true); // ノーマルマップ
				metallic = loadGltfTextureCached(prim.material->pbr_metallic_roughness.metallic_roughness_texture, true); // メタリック
				emissive = loadGltfTextureCached(prim.material->emissive_texture, false); // 自己発光

				sub.material.textures[MaterialTex::BaseColor] = baseColor;
				sub.material.textures[MaterialTex::Normal] = normalMap;
				sub.material.textures[MaterialTex::MetallicRoughness] = metallic;
				sub.material.textures[MaterialTex::Emissive] = emissive;
				sub.material.baseColorFactor = ToVec4(prim.material->pbr_metallic_roughness.base_color_factor);
				sub.material.metallic = prim.material->pbr_metallic_roughness.metallic_factor;
				sub.material.roughness = prim.material->pbr_metallic_roughness.roughness_factor;
				sub.material.emissiveFactor = ToVec3(prim.material->emissive_factor);
				sub.material.doubleSided = prim.material->double_sided != 0;	// cgltf_boolをboolへ明示的な変換

				// AlphaModeの変換
				switch (prim.material->alpha_mode)
				{
				case cgltf_alpha_mode_mask:
					sub.material.alphaMode = MaterialAlphaMode::Mask;
					break;
				case cgltf_alpha_mode_blend:
					sub.material.alphaMode = MaterialAlphaMode::Blend;
					break;
				case cgltf_alpha_mode_opaque:
				default:
					sub.material.alphaMode = MaterialAlphaMode::Opaque;
					break;
				}
				sub.material.alphaCutoff = prim.material->alpha_cutoff;
			}

			// BaseColorハンドルが無効なら白にする
			if (!sub.material.textures[MaterialTex::BaseColor].IsValid())
			{
				sub.material.textures[MaterialTex::BaseColor] = defaultTexture;
			}

			// 自分のテクスチャを登録していく(同じTextureを重複登録しない)
			if (baseColor.IsValid() && baseColor != defaultTexture && std::find(modelData.ownedTextures.begin(), modelData.ownedTextures.end(), baseColor) == modelData.ownedTextures.end())
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

		// BoneではないがBoneの子孫になっているnodeをsocketとして集める
		for (cgltf_size i = 0; i < data->nodes_count; i++)
		{
			if (nodeToBone[i] >= 0) continue; // Bone自身は対象外

			const cgltf_node* socketNode{ &data->nodes[i] };
			if (!socketNode->name) continue; // 名前がないと探せない
			
			const AncestorBone socketBone{ FindAncestorBone(socketNode, data, nodeToBone) };
			if (socketBone.boneIndex < 0) continue; // Boneの下にいないnode

			ModelSocket socket{};
			socket.boneIndex = socketBone.boneIndex;
			socket.name = socketNode->name;
			socket.localToBone = socketBone.localToBone;
			modelData.sockets.push_back(socket);
		}

		if (data->animations_count > 0)
		{
			LoadAnimation(data, modelData.animations);
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


RTHandle GraphicsResourceManager::CreateRenderTarget(UINT _width, UINT _height)
{
	// 満杯ガードを入れる
	if (renderTargetFreeList.empty() && rtSlots.size() >= MAX_RENDER_TARGET_COUNT)
	{
		DEBUG_LOG_ERROR("RenderTargetの登録上限に達しました\n");
		return RTHandle{};
	}

	if (!device || _width == 0 || _height == 0)
	{
		DEBUG_LOG_ERROR("RenderTargetの作成条件が不正です\n");

		return RTHandle{};
	}

	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 描画先なのでTexture2D
	resourceDesc.Width = _width; // 横幅
	resourceDesc.Height = _height; // 縦幅
	resourceDesc.DepthOrArraySize = 1; // 深度または配列数
	resourceDesc.MipLevels = 1; // ミップマップのレベル数
	resourceDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 画素フォーマット
	resourceDesc.SampleDesc.Count = 1; // マルチサンプル数いったんMSAAを使用しない
	resourceDesc.SampleDesc.Quality = 0; // マルチサンプルの品質MSAAが無効なので0
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // メモリ上のデータ配列pattern

	// このリソースを描画先として使用可能にする
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	ComPtr<ID3D12Resource> resource{};

	// 最適化クリア値を指定する
	D3D12_CLEAR_VALUE optimizedClearValue{};
	optimizedClearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 作成するresourceとRTVのformatに合わせsる
	// 普段ClearRenderTargetViewへ渡す色(黒をデフォルトとする)
	optimizedClearValue.Color[0] = 0.0f;
	optimizedClearValue.Color[1] = 0.0f;
	optimizedClearValue.Color[2] = 0.0f;
	optimizedClearValue.Color[3] = 1.0f;
	// 作成直後は描画先として使用する
	HRESULT result{ device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &optimizedClearValue, IID_PPV_ARGS(&resource)) };
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return RTHandle{};
	}

	// RTVとSRVを作る
	DescriptorHandle rtv{ DescriptorManager::Instance().Allocate(HeapType::RTV) }; // RTVで確保
	if (!rtv.IsValid())
	{
		DEBUG_LOG_ERROR("RTVでのDescriptorHandleの確保に失敗しました\n");
		return RTHandle{};
	}
	DescriptorHandle srv{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // SRVで確保
	if (!srv.IsValid())
	{
		DEBUG_LOG_ERROR("SRVでのDescriptorHandleの確保に失敗しました\n");
		DescriptorManager::Instance().Free(HeapType::RTV, rtv); // SRV確保に失敗したので先に取ったRTVを返す
		return RTHandle{};
	}

	// RTVの作成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	device->CreateRenderTargetView(resource.Get(), &rtvDesc, rtv.cpu); // 作成

	// SRVの作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(resource.Get(), &srvDesc, srv.cpu);

	RenderTargetData data{};
	data.resource = resource;
	data.width = _width;
	data.height = _height;
	data.rtvHandle = rtv;
	data.srvHandle = srv;


	int index;
	// 空ではないなら再利用する
	if (!renderTargetFreeList.empty())
	{
		index = renderTargetFreeList.top(); // freelistから取り出す
		renderTargetFreeList.pop(); // 削除
		rtSlots[index].data = data; // Unload時点で++されるので世代は据え置き
	}
	// 空なら伸ばす
	else
	{
		// 新しく追加するので現在のスロット数を新しいindexとする
		index = static_cast<int>(rtSlots.size());
		rtSlots.push_back({ data, 0 }); // 新規なので世代は0で
	}

	int packed{ Pack(index, rtSlots[index].generation) }; // パックしたハンドルを入れる

	return RTHandle(PassKey{}, packed);
}

GPUBufferHandle GraphicsResourceManager::CreateGPUBuffer(const GPUBufferDesc& _desc, const void* _initialData, std::size_t _initialDataSize)
{
	if (!device)
	{
		DEBUG_LOG_ERROR("Deviceが初期化されていません\n");
		return GPUBufferHandle{};
	}

	// 一つのBufferResourceが持てる最大のテクセル数を計算する(2^27)
	constexpr std::size_t MAX_ELEMENT_COUNT{ std::size_t{1} << D3D12_REQ_BUFFER_RESOURCE_TEXEL_COUNT_2_TO_EXP };

	if (_desc.elementCount == 0 || _desc.elementCount > MAX_ELEMENT_COUNT)
	{
		DEBUG_LOG_ERROR("GPUBufferの要素数の値が不正です\n");
		return GPUBufferHandle{};
	}

	// StructuredBufferの要素幅は4byteの倍数で2048byte
	if (_desc.elementStride == 0 || _desc.elementStride > D3D12_REQ_MULTI_ELEMENT_STRUCTURE_SIZE_IN_BYTES || _desc.elementStride % 4 != 0)
	{
		DEBUG_LOG_ERROR("GPUBufferのデータサイズが不正です\n");
		return GPUBufferHandle{};
	}

	// 掛け算を行う前にオーバーフロー確認を行う
	if (_desc.elementCount > (std::numeric_limits<std::size_t>::max)() / _desc.elementStride)
	{
		DEBUG_LOG_ERROR("GPUBufferが全体サイズを超えています\n");
		return GPUBufferHandle{};
	}

	// 全体のバイト数
	std::size_t sizeInBytes{ _desc.elementCount * _desc.elementStride };

	// 初期データなしはnullptrと0,ある場合はBuffer全体のサイズを要求する
	if ((!_initialData && _initialDataSize != 0) || (_initialData && _initialDataSize != sizeInBytes))
	{
		DEBUG_LOG_ERROR("渡されたデータが不正です\n");
		return GPUBufferHandle{};
	}

	const bool canRegister{ !gpuBufferFreeList.empty() || gpuBufferSlots.size() < MAX_GPU_BUFFER_COUNT };
	if (!canRegister)
	{
		DEBUG_LOG_ERROR("GPUBufferの登録上限に達しました\n");
		return GPUBufferHandle{};
	}

	// 失敗した際に破棄できるように一時データ
	GPUBufferData data{};
	data.desc = _desc;
	data.sizeInBytes = static_cast<UINT64>(sizeInBytes);
	data.currentState = D3D12_RESOURCE_STATE_COMMON; // 最初はCommon

	// GPUが書き込む場合でのみUAVを作成を許可する
	const D3D12_RESOURCE_FLAGS resourceFlag{ _desc.allowUnorderedAccess ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS : D3D12_RESOURCE_FLAG_NONE };
	CD3DX12_HEAP_PROPERTIES heapProperties{ D3D12_HEAP_TYPE_DEFAULT }; // デフォルトヒープ指定

	const CD3DX12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(data.sizeInBytes, resourceFlag) };

	const HRESULT result{
		device->CreateCommittedResource(
			&heapProperties, // GPU処理用のDefaultHeap
			D3D12_HEAP_FLAG_NONE,
			&resourceDesc, // 全体サイズの書き込み許可
			data.currentState,
			nullptr,
			IID_PPV_ARGS(data.resource.GetAddressOf())
		)
	};
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("リソースの作成に失敗しました\n");
		return GPUBufferHandle{};
	}

	DescriptorManager& descriptorManager{ DescriptorManager::Instance() }; // キャッシュ

	// まだGPUへ渡していない作成途中のDescriptorだけを返す
	const auto releaseDescriptors
	{
		[&]()
			{
				if (data.srvHandle.IsValid())
				{
					descriptorManager.Free(HeapType::CBV_SRV_UAV, data.srvHandle);
					data.srvHandle = {};
				}
				if (data.uavHandle.IsValid())
				{
					descriptorManager.Free(HeapType::CBV_SRV_UAV, data.uavHandle);
					data.uavHandle = {};
				}
			}
	};

	// 読み取り用のSRVの置き場所の確保
	data.srvHandle = descriptorManager.Allocate(HeapType::CBV_SRV_UAV);
	if (!data.srvHandle.IsValid())
	{
		DEBUG_LOG_ERROR("GPUBufferのSRVハンドルを確保できませんでした\n");
		return GPUBufferHandle{};
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造体の並びなので指定しない
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Buffer.FirstElement = 0; // 先頭要素から読む
	srvDesc.Buffer.NumElements = static_cast<UINT>(_desc.elementCount); // 要素数
	srvDesc.Buffer.StructureByteStride = static_cast<UINT>(_desc.elementStride); // 次の要素までは何Byteか
	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	// Allocateで確保したDescriptorの位置にcpuハンドルを使って指定vaiewを作成
	device->CreateShaderResourceView(data.resource.Get(), &srvDesc, data.srvHandle.cpu);

	// 書き込みが許可されているBufferはUAVを作成する
	if (_desc.allowUnorderedAccess)
	{
		// 書き込み用のUAVの置き場所の確保
		data.uavHandle = descriptorManager.Allocate(HeapType::CBV_SRV_UAV);
		if (!data.uavHandle.IsValid())
		{
			DEBUG_LOG_ERROR("GPUBufferのUAVのハンドルを確保できませんでした\n");
			releaseDescriptors();
			return  GPUBufferHandle{};
		}

		D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
		uavDesc.Format = DXGI_FORMAT_UNKNOWN; // 構造体の並びなので指定しない
		uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
		uavDesc.Buffer.FirstElement = 0;
		uavDesc.Buffer.NumElements = static_cast<UINT>(_desc.elementCount);
		uavDesc.Buffer.StructureByteStride = static_cast<UINT>(_desc.elementStride);
		uavDesc.Buffer.CounterOffsetInBytes = 0;
		uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;

		// SRVと同様にcpuハンドルを使って指定viewを作成 要素数カウンターは無し
		device->CreateUnorderedAccessView(data.resource.Get(), nullptr, &uavDesc, data.uavHandle.cpu);
	}

	// 初期データがあればUploadSystemを使って転送予約
	if (_initialData)
	{
		UploadResult uploadResult{ uploadSystem.EnqueueBuffer(data.resource.Get(), _initialData, sizeInBytes, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON) };

		switch (uploadResult)
		{
		case UploadResult::Success:
			break;
		case UploadResult::OverBudget:
			// 現状溜まっている記録を送信して再利用できる状態にする
			if (!FlushUploads(uploadSystem))
			{
				DEBUG_LOG_ERROR("溜まっている記録の送信に失敗しました\n");
				releaseDescriptors();
				return GPUBufferHandle{};
			}
			// もう一度転送予約
			uploadResult = uploadSystem.EnqueueBuffer(data.resource.Get(), _initialData, sizeInBytes, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COMMON);
			if (uploadResult != UploadResult::Success)
			{
				DEBUG_LOG_ERROR("再度試しましたがGPUBufferへの転送予約ができませんでした\n");
				releaseDescriptors();
				return GPUBufferHandle{};
			}
			break;
		case UploadResult::OtherFailed:
			DEBUG_LOG_ERROR("GPUBufferへの初期転送予約ができませんでした\n");
			releaseDescriptors();
			return GPUBufferHandle{};
		}
	}

	// すべて成功したので追加を行う
	int index{};
	if (!gpuBufferFreeList.empty())
	{
		// 解放済みSlotの利用
		index = gpuBufferFreeList.top();
		gpuBufferFreeList.pop();
		gpuBufferSlots[index].data = std::move(data);
	}
	else
	{
		// 新しいSlotを作る
		index = static_cast<int>(gpuBufferSlots.size());
		gpuBufferSlots.push_back({ std::move(data), 0 });
	}

	int packed{ Pack(index, gpuBufferSlots[index].generation) };
	return GPUBufferHandle{ PassKey{}, packed };
}

TexHandle GraphicsResourceManager::CreateUAVTexture(const TextureDesc& _desc)
{
	if (!device)
	{
		DEBUG_LOG_ERROR("Deviceが初期化されていません\n");
		return TexHandle{};
	}

	// 幅と高さが下限と上限内にいるかチェックするラムダ
	const auto& isInRange
	{
		[](int _value) { return _value > 0 && _value <= D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION; }
	};

	if (!isInRange(_desc.width) || !isInRange(_desc.height))
	{
		DEBUG_LOG_ERROR("設定された高さもしくは幅が不正です\n");
		return TexHandle{};
	}

	// 登録上限チェック
	if (texFreeList.empty() && texSlots.size() >= MAX_TEXTURE_COUNT)
	{
		DEBUG_LOG_ERROR("Textureが登録上限に達しています\n");
		return TexHandle{};
	}

	// descの独自formatをDirectX12に変換する
	const auto& changeDXGIFormat{
		[](TextureFormat _format) -> DXGI_FORMAT
		{
			switch (_format)
			{
			case TextureFormat::RGBA8_UNORM:
				return DXGI_FORMAT_R8G8B8A8_UNORM; // UAV用はSRGBではない
			case TextureFormat::RGBA16_FLOAT:
				return DXGI_FORMAT_R16G16B16A16_FLOAT;
			case TextureFormat::R32_FLOAT:
				return DXGI_FORMAT_R32_FLOAT;
			default:
				DEBUG_LOG_ERROR("不正なFormatです\n");
				return DXGI_FORMAT_UNKNOWN;
			}
		}
	};

	DXGI_FORMAT format{ changeDXGIFormat(_desc.format) };
	if (format == DXGI_FORMAT_UNKNOWN) return TexHandle{}; // ラムダでログを出すので省略

	TextureData data{}; // 失敗したときでもローカルなので消える
	data.currentState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;
	data.height = _desc.height;
	data.width = _desc.width;

	D3D12_HEAP_PROPERTIES heapProps{}; // ヒープのプロパティ設定
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT; // UAVで書き込むのでDefault
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN; // ページングなし

	ComPtr<ID3D12Resource> resource{}; // 作成するリソース
	D3D12_RESOURCE_DESC resDesc{}; // リソース設定構造体
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS; // uav書き込みを許可する
	resDesc.Width = data.width;
	resDesc.Height = data.height;
	resDesc.DepthOrArraySize = 1; // 配列ではない
	resDesc.MipLevels = 1; // ミップマップなし
	resDesc.Format = format;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0; // MSAAはなし
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // GPUに適切な並び方をさせる

	HRESULT result{};
	result = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, data.currentState, nullptr, IID_PPV_ARGS(&resource));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("リソースの作成に失敗しました\n");
		return TexHandle{};
	}
	data.resource = resource; // 作成に成功したので入れる

	// 作成に失敗したときは返せるように両方のハンドルを自動的にチェックして有効ならDescriptorを解放する
	DescriptorManager& descriptorManager{ DescriptorManager::Instance() };
	const auto releaseDescriptors
	{
		[&]()
			{
				if (data.srvHandle.IsValid())
				{
					descriptorManager.Free(HeapType::CBV_SRV_UAV, data.srvHandle);
					data.srvHandle = {};
				}
				if (data.uavHandle.IsValid())
				{
					descriptorManager.Free(HeapType::CBV_SRV_UAV, data.uavHandle);
					data.uavHandle = {};
				}
			}
	};

	// 読み取り用のSRVの置き場所の確保
	data.srvHandle = descriptorManager.Allocate(HeapType::CBV_SRV_UAV);
	if (!data.srvHandle.IsValid())
	{
		DEBUG_LOG_ERROR("TextureのSRVハンドルを確保できませんでした\n");
		return TexHandle{};
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;

	// Allocateで確保したDescriptorの位置にcpuハンドルを使って指定viewを作成
	device->CreateShaderResourceView(data.resource.Get(), &srvDesc, data.srvHandle.cpu);

	// 書き込み用のUAVの置き場所の確保
	data.uavHandle = descriptorManager.Allocate(HeapType::CBV_SRV_UAV);
	if (!data.uavHandle.IsValid())
	{
		DEBUG_LOG_ERROR("TextureのUAVのハンドルを確保できませんでした\n");
		releaseDescriptors();
		return  TexHandle{};
	}

	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
	uavDesc.Format = format;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;

	device->CreateUnorderedAccessView(data.resource.Get(), nullptr, &uavDesc, data.uavHandle.cpu);

	// すべて成功したので追加を行う
	int index{};
	if (!texFreeList.empty())
	{
		// 解放済みSlotの利用
		index = texFreeList.top();
		texFreeList.pop();
		texSlots[index].data = std::move(data);
	}
	else
	{
		// 新しいSlotを作る
		index = static_cast<int>(texSlots.size());
		texSlots.push_back({ std::move(data), 0 });
	}

	int packed{ Pack(index, texSlots[index].generation) };
	return TexHandle{ PassKey{}, packed };
}

bool GraphicsResourceManager::ReadBackBuffer(GPUBufferHandle _handle, void* _dst, std::size_t _size)
{
	if (!_handle.IsValid())
	{
		DEBUG_LOG_ERROR("不正なHandleが渡されました\n");
		return false;
	}

	GPUBufferData* gpuBufferData{ Lookup(_handle) };
	if (!gpuBufferData)
	{
		DEBUG_LOG_ERROR("GPUBufferHandleのLookupに失敗しました\n");
		return false;
	}

	if (!_dst)
	{
		DEBUG_LOG_ERROR("読み戻し先が不正です\n");
		return false;
	}

	if (_size == 0 || gpuBufferData->sizeInBytes < _size)
	{
		DEBUG_LOG_ERROR("サイズが不正です\n");
		return false;
	}

	HRESULT result{};
	// ReadbackBufferの作成
	ComPtr<ID3D12Resource> readbackBuffer{};
	const CD3DX12_HEAP_PROPERTIES readbackProps{ D3D12_HEAP_TYPE_READBACK };
	const CD3DX12_RESOURCE_DESC readbackDesc{ CD3DX12_RESOURCE_DESC::Buffer(_size) }; // readbackにはBufferしか置けない
	result = device->CreateCommittedResource(
		&readbackProps,
		D3D12_HEAP_FLAG_NONE,
		&readbackDesc,
		D3D12_RESOURCE_STATE_COPY_DEST, // readbackのBufferはこれで固定となり動かさない
		nullptr,
		IID_PPV_ARGS(&readbackBuffer));

	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("Compute用ReadbackBufferの作成に失敗しました\n");
		return false;
	}

	bool didTransition{ false };
	// 現在のStateを見てCOPY_SOURCEを含んでいなければ遷移してから記録する
	const auto& recordCopyBuffer
	{
		[&](ID3D12GraphicsCommandList* _cmd)
		{
			// 現在のStateにCOPY_SOURCEが含まれてい無ければ遷移する
			if ((gpuBufferData->currentState & D3D12_RESOURCE_STATE_COPY_SOURCE) == 0)
			{
				D3D12_RESOURCE_BARRIER barrier{};
				barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
				barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
				barrier.Transition.pResource = gpuBufferData->resource.Get();
				barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
				barrier.Transition.StateBefore = gpuBufferData->currentState;
				barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
				_cmd->ResourceBarrier(1, &barrier);
				didTransition = true;
			}

			// GpuBufferDataのリソースをReadbackBufferへコピーする
			_cmd->CopyBufferRegion(readbackBuffer.Get(), 0, gpuBufferData->resource.Get(), 0, _size);
		}
	};

	result = GraphicsDevice::Instance().ExecuteImmediate(recordCopyBuffer);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("読み戻しのGPU実行に失敗しました\n");
		return false;
	}

	// 遷移を終えたのでCOPY_SOURCEにする
	if (didTransition) gpuBufferData->currentState = D3D12_RESOURCE_STATE_COPY_SOURCE; // GPUで実行後Commonに減衰するがCOPY_SOURCEに昇格できるのでFlushも動く

	// ReadbackBufferをMapしてCPUで読めるようにする
	void* cpuData{ nullptr };
	D3D12_RANGE mapRange{ 0, _size };
	D3D12_RANGE unmapRange{ 0, 0 };
	result = readbackBuffer.Get()->Map(0, &mapRange, &cpuData); // CPUがメモリを読めるようにする
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("Compute用ReadbackBufferのMapに失敗しました\n");
		return false;
	}
	std::memcpy(_dst, cpuData, _size);
	readbackBuffer.Get()->Unmap(0, &unmapRange); // CPUがメモリを読めないように閉じる
	return true;
}

ShaderHandle GraphicsResourceManager::RegisterShader(ShaderUsage _usage, ShaderStage _stage, ComPtr<ID3DBlob> _shaderBlob)
{
	if (!_shaderBlob)
	{
		DEBUG_LOG_ERROR("登録するShaderがnullです\n");
		return ShaderHandle{};
	}

	const bool canRegister{ !shaderFreeList.empty() || shaderSlots.size() < MAX_CUSTOM_SHADER_COUNT };
	if (!canRegister)
	{
		DEBUG_LOG_ERROR("Shaderの登録上限に達しました\n");
		return ShaderHandle{};
	}

	ShaderData data{};
	data.usage = _usage;
	data.stage = _stage;
	data.blob = std::move(_shaderBlob);
	int index{};
	if (!shaderFreeList.empty())
	{
		// 解放済みSlotの利用
		index = shaderFreeList.top();
		shaderFreeList.pop();
		shaderSlots[index].data = std::move(data);
	}
	else
	{
		// 新しいSlotを作る
		index = static_cast<int>(shaderSlots.size());
		shaderSlots.push_back({ std::move(data), 0 });
	}

	int packed{ Pack(index, shaderSlots[index].generation) };
	return ShaderHandle{ PassKey{}, packed };
}

MaterialHandle GraphicsResourceManager::RegisterMaterial(ShaderHandle _shader, ComPtr<ID3D12PipelineState> _pipelineState, const MaterialRenderState& _renderState)
{
	ShaderData* shaderData{ Lookup(_shader) };
	if (!shaderData)
	{
		DEBUG_LOG_ERROR("Materialの作成元shaderが無効です\n");
		return MaterialHandle{};
	}

	if (!_pipelineState)
	{
		DEBUG_LOG_ERROR("無効なPSOが渡されています\n");
		return MaterialHandle{};
	}

	const bool canRegister{ !materialFreeList.empty() || materialSlots.size() < MAX_MATERIAL_COUNT };
	if (!canRegister)
	{
		DEBUG_LOG_ERROR("Materialの登録上限に達しました\n");
		return MaterialHandle{};
	}


	MaterialData data{};
	data.pipelineSource = MaterialPipelineSource::Custom; // この時点でpipelineStateが非nullであるのでSourceがCustom,Stateがnullは起こらない
	data.usage = shaderData->usage;
	data.pipelineState = std::move(_pipelineState);
	data.renderState = _renderState; // 台帳に記録
	int index{};
	if (!materialFreeList.empty())
	{
		// 解放済みSlotの利用
		index = materialFreeList.top();
		materialFreeList.pop();
		materialSlots[index].data = std::move(data);
	}
	else
	{
		// 新しいSlotを作る
		index = static_cast<int>(materialSlots.size());
		materialSlots.push_back({ std::move(data), 0 });
	}

	int packed{ Pack(index, materialSlots[index].generation) };
	return MaterialHandle{ PassKey{}, packed };
}

MaterialHandle GraphicsResourceManager::CloneMaterial(MaterialHandle _source)
{
	MaterialData* source{ Lookup(_source) };
	if (!source)
	{
		DEBUG_LOG_ERROR("渡されたMaterialが不正です\n");
		return MaterialHandle{};
	}

	const bool canRegister{ !materialFreeList.empty() || materialSlots.size() < MAX_MATERIAL_COUNT };
	if (!canRegister)
	{
		DEBUG_LOG_ERROR("Materialの登録上限に達しました\n");
		return MaterialHandle{};
	}

	MaterialData data{ *source }; // push_backで配列が伸びるとsourceが無効になるので、先に写す
	int index{};
	if (!materialFreeList.empty())
	{
		index = materialFreeList.top();
		materialFreeList.pop();
		materialSlots[index].data = std::move(data);
	}
	else
	{
		index = static_cast<int>(materialSlots.size());
		materialSlots.push_back({ std::move(data), 0 });
	}

	return MaterialHandle{ PassKey{}, Pack(index, materialSlots[index].generation) };
}

bool GraphicsResourceManager::SetMaterialParameter(MaterialHandle _material, size_t _slot, const void* _data, size_t _dataSize)
{
	MaterialData* material{ Lookup(_material) };
	if (!material)
	{
		DEBUG_LOG_ERROR("SetMaterialParameterに無効なハンドルが渡されました\n");
		return false;
	}
	if (!_data)
	{
		DEBUG_LOG_ERROR("MaterialParameterのdataがnullです\n");
		return false;
	}
	if (_dataSize == 0 || _dataSize > MAX_MATERIAL_PARAMETER_SIZE)
	{
		DEBUG_LOG_ERROR("MaterialParameterのデータサイズが不正です\n");
		return false;
	}
	if (_slot >= MATERIAL_PARAMETER_SLOT_COUNT)
	{
		DEBUG_LOG_ERROR("MaterialParameterのスロット番号が範囲外です : Slot = {}\n", _slot);
		return false;
	}
	MaterialParameterBlock& parameter{ material->parameters[_slot] };
	// 前回より小さいデータを設定した場合でも古い値が後方に残らないように全領域をクリアする
	parameter.parameterData.fill(std::byte{ 0 });
	// ユーザーの構造体をMaterialのCPU保管庫へコピーする
	std::memcpy(parameter.parameterData.data(), _data, _dataSize);
	parameter.parameterSize = _dataSize;
	parameter.hasParameter = true; // 構造体を設定したのでtrue
	return true;
}

bool GraphicsResourceManager::SetMaterialTexture(MaterialHandle _material, std::size_t _slot, TexHandle _texture)
{
	if (_slot >= MATERIAL_RESOURCE_PARAMETER_SLOT_COUNT)
	{
		DEBUG_LOG_ERROR("設定されたSlot数が不正です\n");
		return false;
	}

	MaterialData* materialData{ Lookup(_material) };
	if (!materialData)
	{
		DEBUG_LOG_ERROR("MaterilaのLookupに失敗しました\n");
		return false;
	}

	// 使用用途の確認
	switch (materialData->usage)
	{
	case ShaderUsage::Model:
	case ShaderUsage::PostEffect:
	case ShaderUsage::Sprite:
	case ShaderUsage::VFX:
		break;
	case ShaderUsage::Compute:
	default:
		DEBUG_LOG_ERROR("不正なUsageがMaterialに設定されています\n");
		return false;
	}

	MaterialResourceSlot materialSlot{};
	materialSlot.texture = _texture;
	materialSlot.gpuBuffer = {}; // 明示的に空にする

	// あえてIsValid検査をしていないのは描画時に空ならnullSRV,Lookup失敗ならErrorTextureを区別するため
	materialData->resourceSlots[_slot] = materialSlot;
	return true;
}

bool GraphicsResourceManager::SetMaterialBuffer(MaterialHandle _material, std::size_t _slot, GPUBufferHandle _gpuBuffer)
{
	if (_slot >= MATERIAL_RESOURCE_PARAMETER_SLOT_COUNT)
	{
		DEBUG_LOG_ERROR("設定されたSlot数が不正です\n");
		return false;
	}

	MaterialData* materialData{ Lookup(_material) };
	if (!materialData)
	{
		DEBUG_LOG_ERROR("MaterilaのLookupに失敗しました\n");
		return false;
	}

	// 使用用途の確認
	switch (materialData->usage)
	{
	case ShaderUsage::Model:
	case ShaderUsage::PostEffect:
	case ShaderUsage::Sprite:
	case ShaderUsage::VFX:
		break;
	case ShaderUsage::Compute:
	default:
		DEBUG_LOG_ERROR("不正なUsageがMaterialに設定されています\n");
		return false;
	}

	MaterialResourceSlot materialSlot{};
	materialSlot.texture = {}; // 明示的に空にする
	materialSlot.gpuBuffer = _gpuBuffer;

	materialData->resourceSlots[_slot] = materialSlot;
	return true;
}

ComputePipelineHandle GraphicsResourceManager::RegisterComputePipeline(ComputePipelineData _data)
{
	if (!_data.pipelineState || !_data.rootSignature)
	{
		DEBUG_LOG_ERROR("dataに不正な値が設定されています\n");
		return {};
	}

	const bool canRegister{ !computePipelineFreeList.empty() || computePipelineSlots.size() < MAX_COMPUTE_PIPELINE_COUNT };

	if (!canRegister)
	{
		DEBUG_LOG_ERROR("Slot若しくはフリーリストが不正です\n");
		return {};
	}

	int index{};
	if (!computePipelineFreeList.empty())
	{
		// 解放済みSlotの利用
		index = computePipelineFreeList.top();
		computePipelineFreeList.pop();
		computePipelineSlots[index].data = std::move(_data);
	}
	else
	{
		// 新しいSlotを作る
		index = static_cast<int>(computePipelineSlots.size());
		computePipelineSlots.push_back({ std::move(_data), 0 });
	}

	int packed{ Pack(index, computePipelineSlots[index].generation) };
	return ComputePipelineHandle{ PassKey{}, packed };
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
		if (texture.IsValid() && texture != defaultTexture)
		{
			Unload(texture);
		}
	}


	pendingRelease.models.push_back(std::move(model)); // VB,IBも含めComPtrとDescriptorHanldeを解放待ちへ移動
	modelSlots[index].data = ModelData{}; // 空を入れてsubMeshごと破棄
	modelSlots[index].generation++; // 世代を増やして既存を無効化
	modelFreeList.push(index); // freelistへ返す

}

void GraphicsResourceManager::Unload(RTHandle _handle)
{
	RenderTargetData* data{ Lookup(_handle) };
	if (!data)
	{
		// 無効なハンドル
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return;
	}
	int index{ UnpackIndex(_handle.GetRaw(PassKey{})) }; // indexを取り出す

	// ComPtrとDescriptorHanldeを解放待ちへ移動する
	pendingRelease.renderTargets.push_back(std::move(rtSlots[index].data));
	// CPU側のハンドルは無効化する
	rtSlots[index].data = RenderTargetData{};
	rtSlots[index].generation++;
	renderTargetFreeList.push(index);
}

void  GraphicsResourceManager::Unload(ShaderHandle _handle)
{
	// ハンドルの範囲と世代を確認する
	ShaderData* data{ Lookup(_handle) };
	if (!data)
	{
		DEBUG_LOG_ERROR("無効なShaderHandleです\n");
		return;
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };

	// ShaderBlobはCPU側のデータなので即時解放できる
	shaderSlots[index].data = ShaderData{};

	// 古いハンドルを無効にしてスロットを再利用可能にする
	shaderSlots[index].generation++;
	shaderFreeList.push(index);
}

void  GraphicsResourceManager::Unload(MaterialHandle _handle)
{
	// ハンドルの範囲と世代を確認する
	MaterialData* data{ Lookup(_handle) };
	if (!data)
	{
		DEBUG_LOG_ERROR("無効なMaterialHandleです\n");
		return;
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };

	// PSOはGPUが使用中かもしれないため、Fence付き解放待ちへ移す
	if (materialSlots[index].data.pipelineState)
	{
		pendingRelease.pipelineStates.push_back(std::move(materialSlots[index].data.pipelineState));
	}

	// CPU側のMaterial情報を破棄する
	materialSlots[index].data = MaterialData{};

	// 古いハンドルを無効にしてスロットを再利用可能にする
	materialSlots[index].generation++;
	materialFreeList.push(index);
}

void GraphicsResourceManager::Unload(ComputePipelineHandle _handle)
{
	// ハンドルの範囲と世代を確認する
	ComputePipelineData* data{ Lookup(_handle) };
	if (!data)
	{
		DEBUG_LOG_ERROR("無効なComputePipelineHandleです\n");
		return;
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };

	// PSOはGPUが使用中かもしれないため、Fence付き解放待ちへ移す
	pendingRelease.computePipelines.push_back(std::move(computePipelineSlots[index].data));

	// CPU側のComputePipeline情報を破棄する
	computePipelineSlots[index].data = ComputePipelineData{};

	// 古いハンドルを無効にしてスロットを再利用可能にする
	computePipelineSlots[index].generation++;
	computePipelineFreeList.push(index);
}

void GraphicsResourceManager::Unload(GPUBufferHandle _handle)
{
	// ハンドルの範囲と世代を確認する
	GPUBufferData* data{ Lookup(_handle) };
	if (!data)
	{
		DEBUG_LOG_ERROR("無効なGPUBufferHandleです\n");
		return;
	}

	const int index{ UnpackIndex(_handle.GetRaw(PassKey{})) };

	// BufferはGPUが使用中かもしれないため、Fence付き解放待ちへ移す
	pendingRelease.gpuBuffers.push_back(std::move(gpuBufferSlots[index].data));

	// CPU側のGPUBuffer情報を破棄する
	gpuBufferSlots[index].data = GPUBufferData{};

	// 古いハンドルを無効にしてスロットを再利用可能にする
	gpuBufferSlots[index].generation++;

	// Handleへ格納できる世代の間だけSlotを再利用する
	constexpr uint32_t MAX_GENERATION{ static_cast<uint32_t>((std::numeric_limits<int>::max)() >> INDEX_BITS) };
	if (gpuBufferSlots[index].generation <= MAX_GENERATION) gpuBufferFreeList.push(index);
}

D3D12_GPU_DESCRIPTOR_HANDLE GraphicsResourceManager::ResolveMaterialResourceSRV(MaterialResourceSlot _resourceSlot)
{
	if (_resourceSlot.gpuBuffer.IsValid())
	{
		GPUBufferData* gpuBufferData{ Lookup(_resourceSlot.gpuBuffer) };
		if (!gpuBufferData)
		{
			DEBUG_LOG_ERROR("渡されたGPUBufferHandleが不正なデータでした\n");
			return nullBufferSRV.gpu;
		}
		return gpuBufferData->srvHandle.gpu;
	}


	if (!_resourceSlot.texture.IsValid()) return nullTexSRV.gpu; // 空ならnullSRV
	TextureData* data{ Lookup(_resourceSlot.texture) };
	if (!data)
	{
		TextureData* errorData{ Lookup(errorTexture) };
		if (!errorData)
		{
			DEBUG_LOG_ERROR("渡されたハンドルが不正なデータだったのでエラーテクスチャを出力使用としましたがLookupに失敗しました\n");
			return nullTexSRV.gpu; // エラーも失敗したらログを出しつつnullSRVを返す
		}
		return errorData->srvHandle.gpu;
	}
	return data->srvHandle.gpu;
}

TexHandle GraphicsResourceManager::CreateTextureFromScratch(const DirectX::ScratchImage& _scratch, const DirectX::TexMetadata& _meta)
{
	HRESULT result{};

	// 空きスロットもなく、これ以上vectorを伸ばせない場合
	if (texFreeList.empty() && texSlots.size() >= MAX_TEXTURE_COUNT)
	{
		DEBUG_LOG_ERROR("テクスチャの登録上限に達しました\n");
		return TexHandle{};
	}

	// Defaultヒープに空のテクスチャを作る(CreateTextureは非Xbox環境の場合はCOMMONで返す。formatはmetadataのものを保持する)
	ComPtr<ID3D12Resource> texResource{};
	result = DirectX::CreateTexture(device, _meta, texResource.GetAddressOf());
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{};
	}

	// mip/面ごとに1要素のsubresource配列
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	result = DirectX::PrepareUpload(device, _scratch.GetImages(), _scratch.GetImageCount(), _meta, subresources);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result)) return TexHandle{};

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
	texData.currentState = D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE;

	// GPUへ転送記録予約 作られた状態ではCOMMONなのでbeforにはCOMMON
	UploadResult uploadResult{ uploadSystem.EnqueueTexture(texResource.Get(), subresources, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE) };
	switch (uploadResult)
	{
	case UploadResult::Success:
		break;
	case UploadResult::OverBudget:
		// 現状溜まっている記録を送信して再利用できる状態にする
		if (!FlushUploads(uploadSystem))
		{
			DEBUG_LOG_ERROR("溜まっている記録の送信に失敗しました\n");
			DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, texData.srvHandle);
			texData.srvHandle = {};
			return TexHandle{};
		}
		// もう一度転送予約
		uploadResult = uploadSystem.EnqueueTexture(texResource.Get(), subresources, D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_ALL_SHADER_RESOURCE);
		if (uploadResult != UploadResult::Success)
		{
			DEBUG_LOG_ERROR("再度試しましたがTextureのGPU転送記録予約に失敗しました\n");
			DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, texData.srvHandle);
			texData.srvHandle = {};
			return TexHandle{};
		}
		break;
	case UploadResult::OtherFailed:
		DEBUG_LOG_ERROR("TextureのGPU転送記録予約に失敗しました\n");
		DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, texData.srvHandle);
		texData.srvHandle = {};
		return TexHandle{};
	}

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

TexHandle GraphicsResourceManager::LoadTextureFromGltf(const cgltf_texture_view& _texView, const  std::filesystem::path& _modelDir, bool _isData)
{
	if (!_texView.texture || !_texView.texture->image) return TexHandle{}; // 空を返す
	cgltf_image* image{ _texView.texture->image };

	if (image)
	{
		if (image->uri)
		{
			std::filesystem::path texPath{ _modelDir / image->uri };  // フォルダ + ファイル名
			std::string texPathStr{ texPath.string() };
			return LoadTexture(texPathStr.c_str(), _isData);
		}
		else if (image->buffer_view)
		{
			const uint8_t* bytes{ static_cast<const uint8_t*>(image->buffer_view->buffer->data) + image->buffer_view->offset };
			return LoadTextureFromMemory(bytes, image->buffer_view->size, _isData);
		}
	}

	return TexHandle{};
}

TexHandle GraphicsResourceManager::CreateMetaTexture(Vector3 _color)
{
	// 各要素を0-1に制限した色
	Vector3 metaColor{ std::clamp(_color.x, 0.0f, 1.0f), std::clamp(_color.y, 0.0f, 1.0f), std::clamp(_color.z, 0.0f, 1.0f) };

	DirectX::ScratchImage scratch{}; // スクラッチ
	scratch.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 1, 1, 1, 1); // 1x1, 1配列, 1mip

	uint8_t meta[4]{ static_cast<uint8_t>(metaColor.x * 255.0f), static_cast<uint8_t>(metaColor.y * 255.0f), static_cast<uint8_t>(metaColor.z * 255.0f) ,255 }; // 色
	memcpy(scratch.GetPixels(), meta, 4); // 生のメモリに白を書く
	return CreateTextureFromScratch(scratch, scratch.GetMetadata());
}

bool GraphicsResourceManager::RecordPendingUploads(ID3D12GraphicsCommandList* _commandList)
{
	return uploadSystem.RecordPendingUploads(_commandList);
}

bool GraphicsResourceManager::CommitSubmittedUploads(UINT64 _submittedFenceValue)
{
	return uploadSystem.CommitSubmittedUploads(_submittedFenceValue);
}

void GraphicsResourceManager::CollectCompletedUploads(UINT64 _completedFenceValue)
{
	uploadSystem.CollectCompletedUploads(_completedFenceValue);
}

bool GraphicsResourceManager::HasRecordedUploads() const
{
	return uploadSystem.HasRecordedUploads();
}

