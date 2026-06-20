#define INITGUID

#include <filesystem>
#include "../External/Common/d3dx12.h"
#include "../External/DirectXTex/DirectXTex.h"
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