#include "../External/stb_image.h"
#include "DescriptorManager.h"
#include "ResourceManager.h"

void ResourceManager::Initialize(ID3D12Device* _device)
{
	if (_device != nullptr)
	{
		device = _device;
	}
}

GPUBuffer ResourceManager::CreateVertexBuffer(const void* _data, UINT _dataSize, UINT _strideSize)
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

	GPUBuffer buffer{};
	HRESULT result{}; // 結果が成功しているかどうか調べるための変数
	// UploadHeap上にバッファリソースを作成する
	result = device->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&buffer.resource));
	if (FAILED(result)) return buffer; // 失敗していたら終了
	
	// 頂点バッファに頂点情報をコピーする
	void* mappedData{ nullptr }; // dataを詰めるための変数
	result = buffer.resource->Map(0, nullptr, &mappedData); // バッファの仮想アドレスを取得する
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

// 画像の読み込み
TextureData ResourceManager::LoadTexture(const char* _filePath)
{
	TextureData texData{};
	HRESULT result{}; // 結果判定用

	// 画像を読み込む
	int width{ 0 }; // 横幅
	int height{ 0 }; // 縦幅
	int channels{ 0 }; // 色の構成要素数

	unsigned char* pixels{ stbi_load(_filePath, &width, &height, &channels, 4)}; // 各変数にピクセルの幅等を格納していく(色は強制的にRGBAの4チャンネル)
	if (!pixels) return texData; // 空を返す(失敗時)

	// サイズを代入
	texData.width = width;
	texData.height = height;

	// ヒープの設定
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_CUSTOM; // カスタムに設定する
	heapProps.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // ライトバック
	heapProps.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // L0(CPU)から転送を行う

	// リソースの設定
	D3D12_RESOURCE_DESC resDesc{};
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // RGBAフォーマット
	resDesc.Width = texData.width;
	resDesc.Height = texData.height;
	resDesc.DepthOrArraySize = 1; // 2Dかつ配列でないため
	resDesc.SampleDesc.Count = 1; // アンチエイリアシングしない
	resDesc.SampleDesc.Quality = 0; // クオリティは最低
	resDesc.MipLevels = 1; // ミップマップはしないのでミップ数は一つ
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; // 2Dテクスチャ用
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN; // 決定しない
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE; // フラグもなし

	// リソースの作成
	result = device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, nullptr, IID_PPV_ARGS(&texData.resource));

	// 書き込む範囲を作成する
	D3D12_BOX box{ 0, 0, 0, static_cast<UINT>(width), static_cast<UINT>(height), 1 };
	// データ転送
	result = texData.resource->WriteToSubresource(
		0, // サブリソース番号
		&box, // 書き込む範囲
		pixels, // ピクセルデータ
		width * 4, // 1行のバイト数(width * RGBA)
		width * height * 4 // 全体のバイト数
		);
	if (FAILED(result)) return texData;

	// SRVを作成する
	DescriptorHandle srvHandle{ DescriptorManager::Instance().Allocate(HeapType::CBV_SRV_UAV) }; // SRVのハンドルを取得する
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{}; // SRV設定構造体
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;
	device->CreateShaderResourceView(texData.resource.Get(), &srvDesc, srvHandle.cpu);

	texData.srvHandle = srvHandle;

	// 解放
	stbi_image_free(pixels);
	return texData;
}