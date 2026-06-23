#define INITGUID

#include <filesystem>
#include "../External/Common/d3dx12.h"
#include "../External/DirectXTex/DirectXTex.h"
#include "../External/cgltf.h"
#include"../Debug/DebugLogs.h"
#include "../Core/Handle/HandleConstant.h"
#include "GraphicsDevice.h"
#include "DescriptorManager.h"
#include "ResourceManager.h"

#pragma comment(lib, "windowscodecs.lib") // WIC（LoadFromWICFile）
#pragma comment(lib, "ole32.lib")        // COM（CoInitializeEx / CoCreateInstance）

void ResourceManager::Initialize(ID3D12Device* _device)
{
	if (_device != nullptr)
	{
		device = _device;
	}
}

VertexBuffer ResourceManager::CreateVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize)
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
VertexBuffer ResourceManager::CreateDynamicVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize)
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
DynamicBuffer ResourceManager::CreateDynamicBuffer(UINT _dataSize)
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

IndexBuffer ResourceManager::CreateIndexBuffer(const void* _data, UINT _dataSize, UINT _indexCount)
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
ConstantBufferData ResourceManager::CreateConstantBuffer(const void* _data, UINT _dataSize)
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
TexHandle ResourceManager::LoadTexture(const char* _filePath)
{

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

	// Defaultヒープに空のテクスチャを作る(CreateTextureは非Xbox環境の場合はCOMMONで返す。formatはmetadataのものを保持する)
	ComPtr<ID3D12Resource> texResource;
	result = DirectX::CreateTexture(device, metaData, texResource.GetAddressOf());
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{};
	}

	// UpdateSubResourceヘ渡せる形へ変換(mip/面ごとに1要素のsubresource配列)
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	result = DirectX::PrepareUpload(device, scratch.GetImages(), scratch.GetImageCount(), metaData, subresources);
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
			D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(texResource.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) };
			_cmd->ResourceBarrier(1, &barrier);
		}
	);
	DEBUG_ASSERT(SUCCEEDED(result));
	if (FAILED(result))
	{
		return TexHandle{};
	}

	// SRVの作成(メタデータから引っ張ってきたものを使う)
	DescriptorHandle srv{DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV)}; // 確保
	if (!srv.IsValid())
	{
		// 枯渇していた場合の対処
		DEBUG_ASSERT(false);
		return TexHandle{};
	}

	// 設定
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metaData.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // デフォルトの読み込み
	srvDesc.Texture2D.MipLevels = static_cast<UINT>(metaData.mipLevels); // ミップレベルをメタデータから持ってくる
	device->CreateShaderResourceView(texResource.Get(), &srvDesc, srv.cpu);


	TextureData texData{}; // 戻り値用
	texData.resource = texResource;
	texData.srvHandle = srv;
	texData.width = static_cast<int>(metaData.width);
	texData.height = static_cast<int>(metaData.height);

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

TextureData* ResourceManager::Lookup(TexHandle _handle)
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

ModelData* ResourceManager::Lookup(ModelHandle _handle)
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

ModelHandle ResourceManager::LoadModel(const char* _filePath)
{
	cgltf_options options{}; // 全部0(デフォルト挙動)
	cgltf_data* data{ nullptr };
	std::vector<ModelVertex> verticesData{}; // 頂点情報データ
	std::vector<uint32_t> indicesData{}; // インデックスデータ

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
	// 全メッシュの全プリミティブを見る
	for (cgltf_size i = 0; i < data->meshes_count; i++)
	{
		for (cgltf_size j = 0; j < data->meshes[i].primitives_count; j++)
		{
			const cgltf_primitive& prim{ data->meshes[i].primitives[j] }; // 先頭メッシュの先頭プリミティブ

			const cgltf_accessor* positionAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_position, 0) }; // ポジションのアクセサ
			const cgltf_accessor* normalAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_normal, 0) }; // 法線のアクセサ
			const cgltf_accessor* uvAccessor{ cgltf_find_accessor(&prim, cgltf_attribute_type_texcoord, 0) }; // uvのアクセサ
			DEBUG_ASSERT(positionAccessor && normalAccessor && uvAccessor);
			if (!positionAccessor || !normalAccessor || !uvAccessor)
			{
				return ModelHandle{}; // 失敗したら空を返す
			}


			// 頂点数 =Position属性のアクセサのカウント
			// 属性は型で探す必要がある(順不同)
			const cgltf_size vertCount{ positionAccessor->count }; // 頂点数の取得
			const cgltf_size indexCount{ prim.indices ? prim.indices->count : 0 }; // index数の取得
			verticesData.resize(vertCount); // 頂点データサイズ設定
			indicesData.resize(indexCount); // インデックスサイズ設定
			for (cgltf_size k = 0; k < vertCount; k++)
			{
				cgltf_bool readResult{};
				readResult = cgltf_accessor_read_float(positionAccessor, k, verticesData[k].position, 3);
				if (!readResult)
				{
					DEBUG_LOG_WARNING("positionの読み取りに失敗しました。 : 頂点データ{}番目\n", k);
				}
				readResult = cgltf_accessor_read_float(normalAccessor, k, verticesData[k].normal, 3);
				if (!readResult)
				{
					DEBUG_LOG_WARNING("normalの読み取りに失敗しました。 : 頂点データ{}番目\n", k);
				}
				readResult = cgltf_accessor_read_float(uvAccessor, k, verticesData[k].uv, 2);
				if (!readResult)
				{
					DEBUG_LOG_WARNING("uvの読み取りに失敗しました。 : 頂点データ{}番目\n", k);
				}
			}

			for (cgltf_size k = 0; k < indexCount; k++)
			{
				// primitiveのindicesメンバがアクセサの役割を持つのでそれを使う
				indicesData[k] = static_cast<uint32_t>(cgltf_accessor_read_index(prim.indices, k));
			}

			VertexBuffer vertBuffer{ ResourceManager::Instance().CreateVertexBuffer(verticesData.data(), static_cast<UINT>(verticesData.size()) * sizeof(ModelVertex), sizeof(ModelVertex)) }; // 頂点バッファの作成
			IndexBuffer indexBuffer{ ResourceManager::Instance().CreateIndexBuffer(indicesData.data(), static_cast<UINT>(indicesData.size() * sizeof(uint32_t)), static_cast<UINT>(indicesData.size())) }; // インデックスバッファの作成
			if (!vertBuffer.resource || !indexBuffer.resource)
			{
				DEBUG_LOG_WARNING("頂点バッファとインデックスバッファで不正がありました。mesh : {} , primitive {}\n", i, j);
				continue; // 不正がある場合スキップ 
			}

			SubMesh sub{}; // サブメッシュ
			sub.vertexBuffer = vertBuffer;
			sub.indexBuffer = indexBuffer;
			// PBRチェックを入れる
			if (prim.material && prim.material->has_pbr_metallic_roughness
				&& prim.material->pbr_metallic_roughness.base_color_texture.texture
				&& prim.material->pbr_metallic_roughness.base_color_texture.texture->image
				&& prim.material->pbr_metallic_roughness.base_color_texture.texture->image->uri)
			{
				std::filesystem::path texPath{ modelDir / prim.material->pbr_metallic_roughness.base_color_texture.texture->image->uri };
				std::string texPathStr{ texPath.string() }; // ローカルにする
				sub.texture = LoadTexture(texPathStr.c_str());
			}
			modelData.subMeshes.push_back(sub); // 詰め込む
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

	return ModelHandle(PassKey{}, packed);
}

void ResourceManager::Unload(TexHandle _handle)
{
	TextureData* data{ Lookup(_handle) };
	if (!data)
	{
		// 無効なハンドル
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return;
	} 
	int index{ UnpackIndex(_handle.GetRaw(PassKey{})) }; // indexの取り出し

	DescriptorManager::Instance().Free(HeapType::CBV_SRV_UAV, texSlots[index].data.srvHandle); // スロットの返却
	texSlots[index].data = TextureData{}; // null化を行う
	texSlots[index].generation++; // 世代を増やして既存ハンドルを無効化
	texFreeList.push(index); // indexをfreelistに入れる
}

void ResourceManager::Unload(ModelHandle _handle)
{
	ModelData* data{ Lookup(_handle) };
	if (!data)
	{
		// 無効なハンドル
		DEBUG_LOG_ERROR("無効なハンドルです\n");
		return;
	}
	int index{ UnpackIndex(_handle.GetRaw(PassKey{})) }; // indexを取り出す
	for (SubMesh& sub : modelSlots[index].data.subMeshes)
	{
		/*
			頂点バッファやインデックスバッファはComPtrで管理しているので自動Freeされる
		*/
		if (sub.texture.IsValid()) Unload(sub.texture); // テクスチャの開放
	}

	modelSlots[index].data = ModelData{}; // 空を入れてsubMeshごと破棄
	modelSlots[index].generation++; // 世代を増やして既存を無効化
	modelFreeList.push(index); // freelistへ返す
		
}