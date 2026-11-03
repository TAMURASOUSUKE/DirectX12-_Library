#include <utility>
#include <exception>
#include <cstring>
#include <limits>
#include "../External/Common/d3dx12.h"
#include "../Debug/DebugLogs.h"
#include "UploadSystem.h"

bool UploadSystem::Setup(ID3D12Device* _device, UINT64 _pageSize, UINT64 _maxUploadBytes)
{
	if (initialized)
	{
		DEBUG_LOG_ERROR("すでに初期化されています\n");
		return false;
	}

	if (!_device)
	{
		DEBUG_LOG_ERROR("deviceが無効です\n");
		return false;
	}

	if (_pageSize <= 0 || _maxUploadBytes < _pageSize)
	{
		DEBUG_LOG_ERROR("ページサイズが不正です\n");
		return false;
	}

	device = _device;
	pageSize = _pageSize;
	maxUploadBytes = _maxUploadBytes;
	initialized = true;
	return true;
}

void UploadSystem::Shutdown()
{
	if (!initialized) return;

	// Mapを解除する
	for (UploadPage& page : pages)
	{
		if (page.resource && page.mappedData) page.resource->Unmap(0, nullptr); // 0はサブリソース番号
		page.mappedData = nullptr;
	}

	// 空のvectorと入れ替えて要素と管理配列の確保容量を解放
	std::vector<UploadBatch>{}.swap(batches);
	std::vector<UploadPage>{}.swap(pages);

	pendingBatchIndex = INVALID_UPLOAD_INDEX;
	recordedBatchIndex = INVALID_UPLOAD_INDEX;

	pageSize = 0;
	maxUploadBytes = 0;
	allocatedUploadBytes = 0;

	device.Reset();

	initialized = false;
}

UploadResult UploadSystem::EnqueueBuffer(ID3D12Resource* _destination, const void* _sourceData, UINT64 _sizeInBytes, D3D12_RESOURCE_STATES _beforeState, D3D12_RESOURCE_STATES _afterState)
{
	if (!initialized || !device)
	{
		DEBUG_LOG_ERROR("UploadSystemが初期化されていません\n");
		return UploadResult::OtherFailed;
	}

	if (!_destination || !_sourceData || _sizeInBytes == 0)
	{
		DEBUG_LOG_ERROR("Buffer転送の引数が不正です\n");
		return UploadResult::OtherFailed;
	}

	// コピーサイズがsize_tの上限を超えていないかを確認
	constexpr UINT64 maxCopyBytes{ (std::numeric_limits<std::size_t>::max)() };
	if (_sizeInBytes > maxCopyBytes)
	{
		DEBUG_LOG_ERROR("CPUからコピーできるサイズの上限を超えています\n");
		return UploadResult::OtherFailed;
	}

	// 種類とサイズの確認
	const D3D12_RESOURCE_DESC destinationDesc{ _destination->GetDesc() };
	if (destinationDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || destinationDesc.Width != _sizeInBytes)
	{
		DEBUG_LOG_ERROR("転送先がBufferではないか全体サイズが一致していません\n");
		return UploadResult::OtherFailed;
	}

	const std::size_t batchIndex{ AcquirePendingBatch() };
	if (batchIndex == INVALID_UPLOAD_INDEX) return UploadResult::OtherFailed;

	UploadBatch& batch{ batches[batchIndex] };

	// 追加する要素の番号を追加前の個数から取得
	const std::size_t requestIndex{ batch.requests.size() };
	const std::size_t copyIndex{ batch.bufferCopies.size() };
	if (requestIndex >= batch.requests.max_size() || copyIndex >= batch.bufferCopies.max_size())
	{
		DEBUG_LOG_ERROR("Buffer転送の管理配列の上限に達しました\n");
		return UploadResult::OtherFailed;
	}

	// 予約枠を確保
	try
	{
		batch.requests.emplace_back();
		batch.bufferCopies.emplace_back();
	}
	catch (const std::bad_alloc&)
	{
		// 途中まで確保できていた場合は取り除く
		if (batch.requests.size() > requestIndex) batch.requests.pop_back();
		if (batch.bufferCopies.size() > copyIndex) batch.bufferCopies.pop_back();

		DEBUG_LOG_ERROR("Buffer転送の管理情報のメモリ確保に失敗しました\n");
		return UploadResult::OtherFailed;
	}

	UploadAllocation allocation{};

	// 今回はBufferのバイト列コピー用なのでPage内は1バイト境界で確保する
	UploadResult uploadResult{ AllocateUpload(_sizeInBytes, 1, allocation) };
	if (uploadResult != UploadResult::Success)
	{
		// 領域を確保できなかったので、先に用意した空要素を取り除く
		batch.bufferCopies.pop_back();
		batch.requests.pop_back();
		return uploadResult;
	}

	// page追加の可能性を考え確保後に参照取得
	UploadPage& page{ pages[allocation.pageIndex] };

	// 呼び出し元のCPUデータを、UploadSystemが保持する領域へコピーする
	std::memcpy(page.mappedData + static_cast<std::size_t>(allocation.offset), _sourceData, static_cast<std::size_t>(_sizeInBytes));

	// 後でCopyBufferRegionを記録するためのコピー元情報
	BufferCopy& copy{ batch.bufferCopies[copyIndex] };
	copy.pageIndex = allocation.pageIndex;
	copy.sourceOffset = allocation.offset;
	copy.sizeInBytes = _sizeInBytes;

	// 転送先と状態遷移、対応するコピー情報の位置を保存する
	UploadRequest& request{ batch.requests[requestIndex] };
	request.destination = _destination; // ComPtrで転送先Resourceの参照を保持
	request.type = UploadType::Buffer;
	request.beforBuffer = _beforeState; // 現在の構造体のメンバ名
	request.afterBuffer = _afterState;
	request.firstCopyIndex = copyIndex;
	request.copyCount = 1; // Buffer全体を1回のコピーで転送する
	return uploadResult;
}

UploadResult UploadSystem::EnqueueTexture(ID3D12Resource* _destination, std::span<const D3D12_SUBRESOURCE_DATA> _subresources, D3D12_RESOURCE_STATES _beforeState, D3D12_RESOURCE_STATES _afterState)
{
	if (!initialized || !device)
	{
		DEBUG_LOG_ERROR("UploadSystemが初期化されていません\n");
		return UploadResult::OtherFailed;
	}

	if (!_destination || _subresources.empty())
	{
		DEBUG_LOG_ERROR("Texture転送の引数が不正です\n");
		return UploadResult::OtherFailed;
	}

	const D3D12_RESOURCE_DESC destinationDesc{ _destination->GetDesc() };

	// Textureが渡されているか確認
	const bool isTexture{
		destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ||
		destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
		destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D 
	};

	// MSAAは扱わない
	if (!isTexture || destinationDesc.SampleDesc.Count != 1)
	{
		DEBUG_LOG_ERROR("転送先には非MSAAのTextureを指定してください\n");
		return UploadResult::OtherFailed;
	}

	// 転送先はDefaultHeap
	D3D12_HEAP_PROPERTIES heapProperties{};
	D3D12_HEAP_FLAGS heapFlags{};

	const HRESULT heapResult{ _destination->GetHeapProperties(&heapProperties, &heapFlags) };
	if (FAILED(heapResult) || heapProperties.Type != D3D12_HEAP_TYPE_DEFAULT)
	{
		DEBUG_LOG_ERROR("転送先にはDEFAULT HeapのTextureを指定してください\n");
		return UploadResult::OtherFailed;
	}

	// Formatによっては複数のplaneを持つため数を取得
	D3D12_FEATURE_DATA_FORMAT_INFO formatInfo{};
	formatInfo.Format = destinationDesc.Format;

	const HRESULT formatResult{ device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo)) };
	if (FAILED(formatResult) || formatInfo.PlaneCount == 0)
	{
		DEBUG_LOG_ERROR("TextureのFormat情報を取得できませんでした\n");
		return UploadResult::OtherFailed;
	}

	// 3DTextureのDepthは配列数ではないので分ける
	const UINT64 arraySize{ destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : static_cast<UINT64>(destinationDesc.DepthOrArraySize) };
	// Texture全体の転送のため全SubResourceのデータが必要
	const UINT64 expectedCount{ static_cast<UINT64>(destinationDesc.MipLevels) * arraySize * static_cast<UINT64>(formatInfo.PlaneCount) };
	if (expectedCount == 0 || expectedCount > D3D12_REQ_SUBRESOURCES || expectedCount != static_cast<UINT64>(_subresources.size()))
	{
		DEBUG_LOG_ERROR("Texture全体のSubresource数と入力データ数が一致していません\n");
		return UploadResult::OtherFailed;
	}

	const UINT subresourceCount{ static_cast<UINT>(expectedCount) };

	// 今回の転送に必要な配置情報を一時的に受け取る配列
	std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> footprints{};
	std::vector<UINT> rowCounts{};
	std::vector<UINT64> rowSizes{};

	try
	{
		footprints.resize(subresourceCount);
		rowCounts.resize(subresourceCount);
		rowSizes.resize(subresourceCount);
	}
	catch (const std::bad_alloc&)
	{
		DEBUG_LOG_ERROR("Textureの配置情報のメモリ確保に失敗しました\n");
		return UploadResult::OtherFailed;
	}

	UINT64 requiredBytes{ 0 };

	// 先頭位置0を基準に全サブリソースの配置と必要容量を取得する
	device->GetCopyableFootprints(
		&destinationDesc, // 設定 
		0, // 最初のSubresource
		subresourceCount, // 転送するSubresource数
		0, // 配置の基準位置
		footprints.data(), // 各Subresourceの配置情報
		rowCounts.data(), // 各Subresourceのコピーする行数
		rowSizes.data(), // 各行の実データのバイト数
		&requiredBytes // 全体で必要な容量
	);

	constexpr UINT64 maxCopyBytes{ (std::numeric_limits<size_t>::max)() };
	// CPU側のポインタ計算で使える範囲
	constexpr UINT64 maxSourceBytes{ static_cast<UINT64>((std::numeric_limits<LONG_PTR>::max)()) };
	if (requiredBytes == 0 || requiredBytes == (std::numeric_limits<UINT64>::max)() || requiredBytes > maxCopyBytes)
	{
		DEBUG_LOG_ERROR("Texture転送に必要な容量が不正です\n");
		return UploadResult::OtherFailed;
	}

	// 全てのコピー元の検証
	for (UINT i = 0; i < subresourceCount; i++)
	{
		const D3D12_SUBRESOURCE_DATA& source{ _subresources[i] };
		const UINT rows{ rowCounts[i] }; // コピーする行数1px = 4byteで100列分なら400byte
		const UINT depth{ footprints[i].Footprint.Depth }; // 画像の奥行 2Dなら1

		if (!source.pData || source.RowPitch <= 0 || source.SlicePitch <= 0 || rows == 0 || depth == 0 || rowSizes[i] == 0)
		{
			DEBUG_LOG_ERROR("Textureのコピー元データが不正です\n");
			return UploadResult::OtherFailed;
		}

		const UINT64 sourceRowPitch{ static_cast<UINT64>(source.RowPitch) };
		const UINT64 sourceSlicePitch{ static_cast<UINT64>(source.SlicePitch) };

		// 1行の間隔が実データより短い場合はコピーできない
		if (rowSizes[i] > sourceRowPitch || sourceRowPitch > sourceSlicePitch / rows || sourceSlicePitch > maxSourceBytes / depth)
		{
			DEBUG_LOG_ERROR("Textureの行間隔またはスライス間隔が不正です\n");
			return UploadResult::OtherFailed;
		}
	}

	const std::size_t batchIndex{ AcquirePendingBatch() };
	if (batchIndex == INVALID_UPLOAD_INDEX) return UploadResult::OtherFailed;

	UploadBatch& batch{ batches[batchIndex] };

	const std::size_t requestIndex{ batch.requests.size() };
	const std::size_t copyIndex{ batch.textureCopies.size() };
	const std::size_t copyCount{ subresourceCount };

	if (requestIndex >= batch.requests.max_size() || copyCount > batch.textureCopies.max_size() - copyIndex)
	{
		DEBUG_LOG_ERROR("Texture転送の管理配列の上限に達しました\n");
		return UploadResult::OtherFailed;
	}

	// Pageを確保する前に要求とコピー情報の格納場所を容易
	try
	{
		batch.requests.emplace_back();
		batch.textureCopies.resize(copyIndex + copyCount);
	}
	catch (const std::bad_alloc&)
	{
		if (batch.requests.size() > requestIndex) batch.requests.pop_back();
		DEBUG_LOG_ERROR("Texture転送の管理情報のメモリ確保に失敗しました\n");
		return UploadResult::OtherFailed;
	}

	UploadAllocation allocation{};

	// Textureの配置開始位置は512byte境界に合わせる
	UploadResult uploadResult{ AllocateUpload(requiredBytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT, allocation) };
	if(uploadResult != UploadResult::Success)
	{
		batch.textureCopies.resize(copyIndex);
		batch.requests.pop_back();
		return uploadResult;
	}

	// Page追加による再確保が終わった後に参照を取得する
	UploadPage& page{ pages[allocation.pageIndex] };

	for (UINT i = 0; i < subresourceCount; i++)
	{
		const D3D12_SUBRESOURCE_DATA& source{ _subresources[i] };
		D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{ footprints[i] };

		// 配置情報は先頭0基準なので実際に確保した位置を加える
		footprint.Offset += allocation.offset;

		const UINT64 destinationRowPitch{ footprint.Footprint.RowPitch };
		const UINT64 destinationSlicePitch{ destinationRowPitch * rowCounts[i] };

		const UINT64 sourceRowPitch{ static_cast<UINT64>(source.RowPitch) };
		const UINT64 sourceSlicePitch{ static_cast<UINT64>(source.SlicePitch) };

		const std::byte* sourceData{ static_cast<const std::byte*>(source.pData) };
		std::byte* destinationData{ page.mappedData + static_cast<std::size_t>(footprint.Offset) };

		// 2DTextureではDepthが1. 3Dでは奥行分処理
		for (UINT z = 0; z < footprint.Footprint.Depth; z++)
		{
			for (UINT y = 0; y < rowCounts[i]; y++)
			{
				const UINT64 sourceOffset{ sourceSlicePitch * z + sourceRowPitch * y };
				const UINT64 destinationOffset{ destinationSlicePitch * z + destinationRowPitch * y };

				// 余白を含めずに行の実データをコピー
				std::memcpy(
					destinationData + static_cast<std::size_t>(destinationOffset), 
					sourceData + static_cast<std::size_t>(sourceOffset),
					static_cast<std::size_t>(rowSizes[i]));
			}
		}

		// GPUへのコピー命令を後で記録するための情報
		TextureCopy& copy{ batch.textureCopies[copyIndex + i] };
		copy.pageIndex = allocation.pageIndex;
		copy.footprint = footprint;
		copy.subresourceIndex = i;
	}

	// Texture一つ分の要求としてまとめる
	UploadRequest& request{ batch.requests[requestIndex] };
	request.destination = _destination;
	request.type = UploadType::Texture;
	request.beforBuffer = _beforeState;
	request.afterBuffer = _afterState;
	request.firstCopyIndex = copyIndex;
	request.copyCount = copyCount;
	return uploadResult;
}

bool UploadSystem::RecordPendingUploads(ID3D12GraphicsCommandList* _commandList)
{
	if (!initialized || !device || !_commandList)
	{
		DEBUG_LOG_ERROR("UploadSystemまたはCommandListが無効です\n");
		return false;
	}

	// 今回はDirectCommandListを使用
	if (_commandList->GetType() != D3D12_COMMAND_LIST_TYPE_DIRECT)
	{
		DEBUG_LOG_ERROR("転送の記録にはDirect CommandListを指定してください\n");
		return false;
	}

	// 記録済みBatchは送信後のFence値と関連付けるまで保持する
	if (recordedBatchIndex != INVALID_UPLOAD_INDEX)
	{
		DEBUG_LOG_ERROR("前の記録済みBatchがまだ送信確定されていません\n");
		return false;
	}

	// 予約が無ければ何も記録する必要はない
	if (pendingBatchIndex == INVALID_UPLOAD_INDEX) return true;
	if (pendingBatchIndex >= batches.size())
	{
		DEBUG_LOG_ERROR("予約中Batchの番号が不正です\n");
		return false;
	}

	UploadBatch& batch{ batches[pendingBatchIndex] };
	if (batch.state != BatchState::Pending)
	{
		DEBUG_LOG_ERROR("予約中Batchの状態が不正です\n");
		return false;
	}

	// 確保に失敗した後など空のPendingbatchがある場合
	if (batch.requests.empty()) return true;

	// 全予約を検証
	for (const UploadRequest& request : batch.requests)
	{
		if (!request.destination)
		{
			DEBUG_LOG_ERROR("転送先Resourceが無効です\n");
			return false;
		}

		const D3D12_RESOURCE_DESC destinationDesc{ request.destination->GetDesc() };

		// BufferとTextureで共通の転送先チェック
		D3D12_HEAP_PROPERTIES heapProperties{};
		D3D12_HEAP_FLAGS heapFlags{};

		const HRESULT result{ request.destination->GetHeapProperties(&heapProperties, &heapFlags) };
		if (FAILED(result) || heapProperties.Type != D3D12_HEAP_TYPE_DEFAULT)
		{
			DEBUG_LOG_ERROR("転送先にはDEFAULT HeapのResourceを指定してください\n");
			return false;
		}

		// バッファの場合の確認
		if (request.type == UploadType::Buffer)
		{
			if (request.copyCount != 1 || request.firstCopyIndex >= batch.bufferCopies.size())
			{
				DEBUG_LOG_ERROR("Buffer転送予約の内容が不正です\n");
				return false;
			}

			const BufferCopy& copy{ batch.bufferCopies[request.firstCopyIndex] };
			if (copy.pageIndex >= pages.size() || copy.sizeInBytes == 0)
			{
				DEBUG_LOG_ERROR("Bufferコピー情報が不正です\n");
				return false;
			}

			const UploadPage& page{ pages[copy.pageIndex] };
			if (!page.inUse || !page.resource || !page.mappedData || page.usedBytes > page.capacity)
			{
				DEBUG_LOG_ERROR("転送元Pageの管理情報が不正です\n");
				return false;
			}

			// 実際に確保した範囲内をコピーするか確認する
			if (copy.sourceOffset > page.usedBytes || copy.sizeInBytes > page.usedBytes - copy.sourceOffset)
			{
				DEBUG_LOG_ERROR("コピー元の範囲が確保済み領域を超えています\n");
				return false;
			}

			if (destinationDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER || destinationDesc.Width != copy.sizeInBytes)
			{
				DEBUG_LOG_ERROR("転送先Bufferの種類または全体サイズが不正です\n");
				return false;
			}
		}
		// Textureの場合
		else if (request.type == UploadType::Texture)
		{
			const bool isTexture{
				destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D ||
				destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D ||
				destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D
			};

			if (!isTexture || destinationDesc.SampleDesc.Count != 1)
			{
				DEBUG_LOG_ERROR("転送先Textureの種類またはSample数が不正です\n");
				return false;
			}

			// 足し算による桁あふれを避けて配列範囲確認する
			if (request.copyCount == 0 || request.firstCopyIndex > batch.textureCopies.size() || request.copyCount > batch.textureCopies.size() - request.firstCopyIndex)
			{
				DEBUG_LOG_ERROR("Textureコピー情報の配列範囲が不正です\n");
				return false;
			}

			D3D12_FEATURE_DATA_FORMAT_INFO formatInfo{};
			formatInfo.Format = destinationDesc.Format;

			const HRESULT formatResult{ device->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo)) };
			if (FAILED(formatResult) || formatInfo.PlaneCount == 0)
			{
				DEBUG_LOG_ERROR("TextureのFormat情報を取得できませんでした\n");
				return false;
			}

			const UINT64 arraySize{ destinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? 1 : static_cast<UINT64>(destinationDesc.DepthOrArraySize) };
			const UINT64 expectedCount{ static_cast<UINT64>(destinationDesc.MipLevels) * arraySize * static_cast<UINT64>(formatInfo.PlaneCount) };

			// テクスチャの全体の転送を行うので全SubResourceが必要
			if (expectedCount == 0 || expectedCount > D3D12_REQ_SUBRESOURCES || request.copyCount != expectedCount)
			{
				DEBUG_LOG_ERROR("TextureのSubResource数とコピー数が一致していません\n");
				return false;
			}

			for (std::size_t i = 0; i < request.copyCount; i++)
			{
				const TextureCopy& copy{ batch.textureCopies[request.firstCopyIndex + i] };

				// EnqueueTextureはSubresource番号順に登録しているためチェック
				if (copy.subresourceIndex != static_cast<UINT>(i) || copy.pageIndex >= pages.size())
				{
					DEBUG_LOG_ERROR("TextureのSubresource番号またはPage番号が不正です\n");
					return false;
				}

				const UploadPage& page{ pages[copy.pageIndex] };
				if (!page.inUse || !page.resource || !page.mappedData || page.usedBytes > page.capacity)
				{
					DEBUG_LOG_ERROR("転送元Pageの管理情報が不正です\n");
					return false;
				}

				// 範囲を512byte境界かチェックする
				if (copy.footprint.Offset % D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT != 0)
				{
					DEBUG_LOG_ERROR("Textureの配置開始位置が境界にあっていません\n");
					return false;
				}

				// このSubresource単体の正しい配置と必要容量を取得する
				D3D12_PLACED_SUBRESOURCE_FOOTPRINT expectedFootprint{};
				UINT64 requiredBytes{ 0 };
				device->GetCopyableFootprints(
					&destinationDesc,
					copy.subresourceIndex,
					1,
					0,
					&expectedFootprint,
					nullptr,
					nullptr,
					&requiredBytes
				);

				if (requiredBytes == 0 || requiredBytes == (std::numeric_limits<UINT64>::max)())
				{
					DEBUG_LOG_ERROR("Texutreコピーの必要容量を取得できてませんでした\n");
					return false;
				}

				const D3D12_SUBRESOURCE_FOOTPRINT& saved{ copy.footprint.Footprint };
				const D3D12_SUBRESOURCE_FOOTPRINT& expected{ expectedFootprint.Footprint };

				// 保存した配置が転送先Subresourceと一致しているか確認する
				if (saved.Format != expected.Format || saved.Width != expected.Width || saved.Height != expected.Height || saved.Depth != expected.Depth || saved.RowPitch != expected.RowPitch)
				{
					DEBUG_LOG_ERROR("Textureの配置情報が転送先と一致していません\n");
					return false;
				}

				// コピー元の末尾が確保済み領域を超えないか確認する
				if (copy.footprint.Offset > page.usedBytes || requiredBytes > page.usedBytes - copy.footprint.Offset)
				{
					DEBUG_LOG_ERROR("Textureのコピー元が確保済み領域を超えています\n");
					return false;
				}
			}
		}
		else
		{
			DEBUG_LOG_ERROR("転送要求の種類が不正です\n");
			return false;
		}
	}

	// 検証を確認後命令の記録を行う
	for (const UploadRequest& request : batch.requests)
	{
		// 転送先をコピーで書き込める状態へ遷移する
		if (request.beforBuffer != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			const D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
				request.destination.Get(), // 遷移するリソース本体
				request.beforBuffer, // コピー前の状態
				D3D12_RESOURCE_STATE_COPY_DEST // コピー先として使用
			) };

			_commandList->ResourceBarrier(1, &barrier);
		}

		// Bufferの場合の処理
		if (request.type == UploadType::Buffer)
		{
			const BufferCopy& copy{ batch.bufferCopies[request.firstCopyIndex] };
			const UploadPage& page{ pages[copy.pageIndex] };

			// Page内のデータを転送先Bufferの先頭へコピーする命令
			_commandList->CopyBufferRegion(
				request.destination.Get(), // 転送先
				0, // 転送先の開始位置
				page.resource.Get(), // 転送元のUploadBuffer
				copy.sourceOffset, // 転送元の開始位置
				copy.sizeInBytes // コピーする量
			);
		}
		// Textureは転送処理が別なので分ける
		else if (request.type == UploadType::Texture)
		{
			// TextureはSubresourceごとに命令を記録
			for (std::size_t i = 0; i < request.copyCount; i++)
			{
				const TextureCopy& copy{ batch.textureCopies[request.firstCopyIndex + i] };
				const UploadPage& page{ pages[copy.pageIndex] }; 

				// コピー先はTexture内のSubresource番号で指定する
				D3D12_TEXTURE_COPY_LOCATION destinationLocation{};
				destinationLocation.pResource = request.destination.Get();
				destinationLocation.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
				destinationLocation.SubresourceIndex = copy.subresourceIndex;

				// コピー元はBuffer内の画像配置情報でしている
				D3D12_TEXTURE_COPY_LOCATION sourceLoaction{};
				sourceLoaction.pResource = page.resource.Get();
				sourceLoaction.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
				sourceLoaction.PlacedFootprint = copy.footprint;

				_commandList->CopyTextureRegion(
					&destinationLocation, // コピー先Textureの場所
					0, 0, 0, // 書き込み開始位置(X,Y,Z)
					&sourceLoaction, // UploadBuffer内の画像の場所
					nullptr // 切り抜かずにSubresource全体をコピーする
				);
			}
		}

		// Bufferを使用する状態へ
		if (request.afterBuffer != D3D12_RESOURCE_STATE_COPY_DEST)
		{
			const D3D12_RESOURCE_BARRIER barrier{ CD3DX12_RESOURCE_BARRIER::Transition(
				request.destination.Get(), // 遷移するリソース本体
				D3D12_RESOURCE_STATE_COPY_DEST, // コピー前の状態
				request.afterBuffer // コピー先として使用
			) };

			_commandList->ResourceBarrier(1, &barrier);
		}
	}

	// 命令を記録したので状態を合わせる
	batch.state = BatchState::Recorded;
	recordedBatchIndex = pendingBatchIndex;
	pendingBatchIndex = INVALID_UPLOAD_INDEX;
	return true;
}

bool UploadSystem::CommitSubmittedUploads(UINT64 _fenceValue)
{
	if (!initialized)
	{
		DEBUG_LOG_ERROR("UploadSystemが初期化されていません\n");
		return false;
	}

	// 0は未送信を表す初期値なので失敗
	if (_fenceValue == 0 || _fenceValue == (std::numeric_limits<UINT64>::max)())
	{
		DEBUG_LOG_ERROR("送信確定に無効なFence値が渡されました\n");
		return false;
	}
	if (recordedBatchIndex == INVALID_UPLOAD_INDEX || recordedBatchIndex >= batches.size())
	{
		DEBUG_LOG_ERROR("送信確定する記録済みBatchがありません\n");
		return false;
	}

	UploadBatch& batch{ batches[recordedBatchIndex] };
	if (batch.state != BatchState::Recorded)
	{
		DEBUG_LOG_ERROR("送信確定するBatchの状態が不正です\n");
		return false;
	}

	// この値にGPUが到達したらPageを回収
	batch.fenceValue = _fenceValue;

	// 状態をGPU完了待ちへ
	batch.state = BatchState::InFlight;

	// 記録済みBatchの枠を空ける
	recordedBatchIndex = INVALID_UPLOAD_INDEX;
	return true;
}

void UploadSystem::CollectCompletedUploads(UINT64 _completedFenceValue)
{
	if (!initialized)
	{
		DEBUG_LOG_ERROR("UploadSystemが初期化されていません\n");
		return;
	}

	// GetCopletedValueはデバイスが失われた場合にUINT64_MAXを返す
	if (_completedFenceValue == (std::numeric_limits<UINT64>::max)())
	{
		DEBUG_LOG_ERROR("Fenceの完了値が無効です。デバイス状態を確認してください\n");
		return;
	}

	for (std::size_t i = 0; i < batches.size(); i++)
	{
		const UploadBatch& batch{ batches[i] };

		// 送信済みのbatchだけを完了確認する
		if (batch.state != BatchState::InFlight) continue;

		if (batch.fenceValue == 0)
		{
			DEBUG_LOG_ERROR("送信済みBatchに有効なFenceがありません\n");
			return;
		}

		// GPUがまだ完了地点に達していない
		if (_completedFenceValue < batch.fenceValue) continue;

		// 到達を確認したので再利用可能にする
		RecycleBatch(i);
	}
}

bool UploadSystem::HasPendingUploads() const
{
	if (!initialized || pendingBatchIndex == INVALID_UPLOAD_INDEX || pendingBatchIndex >= batches.size()) return false;

	const UploadBatch& batch{ batches[pendingBatchIndex] };
	return batch.state == BatchState::Pending && !batch.requests.empty(); // Pendingかつ空ではないなら存在する
}

bool UploadSystem::HasRecordedUploads() const
{
	if (!initialized || recordedBatchIndex == INVALID_UPLOAD_INDEX || recordedBatchIndex >= batches.size()) return false;

	const UploadBatch& batch{ batches[recordedBatchIndex] };
	return batch.state == BatchState::Recorded && !batch.requests.empty(); // Recordedかつ空ではないなら存在する
}

std::size_t UploadSystem::AcquirePendingBatch()
{
	if (!initialized)
	{
		DEBUG_LOG_ERROR("UploadSystemが初期化されていません\n");
		return INVALID_UPLOAD_INDEX;
	}

	// すでに予約しているBatchがあるならそこに追加する
	if (pendingBatchIndex != INVALID_UPLOAD_INDEX)
	{
		// 番号の範囲と状態確認
		if (pendingBatchIndex >= batches.size() || batches[pendingBatchIndex].state != BatchState::Pending)
		{
			DEBUG_LOG_ERROR("管理状態が不正です\n");
			return INVALID_UPLOAD_INDEX;
		}
		return pendingBatchIndex;
	}

	// GPUでの使用が終了して回収済みになっているBatchを探す
	for (std::size_t i = 0; i < batches.size(); i++)
	{
		UploadBatch& batch{ batches[i] };
		if (batch.state != BatchState::Free) continue;

		// Freeは回収時に要求やPage番号は消してある
		batch.fenceValue = 0; // 新しい転送をまだ送信していないのでFence値は0
		batch.state = BatchState::Pending; // 予約済みへ
		pendingBatchIndex = i;
		return pendingBatchIndex;
	}

	// 配列がこれ以上確保できない場合
	if (batches.size() >= batches.max_size())
	{
		DEBUG_LOG_ERROR("UploadBatchの管理配列の上限に達しました\n");
		return INVALID_UPLOAD_INDEX;
	}

	try
	{
		// 空のBatchを実体配列へ 
		batches.emplace_back();
	}
	catch (const std::bad_alloc&)
	{
		DEBUG_LOG_ERROR("メモリ確保に失敗しました\n");
		return INVALID_UPLOAD_INDEX;
	}

	// 追加に成功を確認してから予約確定
	const std::size_t index{ batches.size() - 1 };
	batches[index].state = BatchState::Pending;

	pendingBatchIndex = index;
	return pendingBatchIndex;
}

UploadResult UploadSystem::AllocateUpload(UINT64 _sizeInBytes, UINT64 _alignment, UploadAllocation& _outAllocation)
{
	// 失敗時は無効な値を返す
	_outAllocation = {};

	if (!initialized || !device)
	{
		DEBUG_LOG_ERROR("UploadSystemが初期化されていません\n");
		return UploadResult::OtherFailed;
	}

	if (_sizeInBytes == 0 || _alignment == 0)
	{
		DEBUG_LOG_ERROR("確保サイズまたは配置境界が0です\n");
		return UploadResult::OtherFailed;
	}

	// 今回の領域を使用するBatchを取得する
	const std::size_t batchIndex{ AcquirePendingBatch() };
	if (batchIndex == INVALID_UPLOAD_INDEX) return UploadResult::OtherFailed;

	UploadBatch& batch{ batches[batchIndex] };

	// Batchが既に使用しているPageの余りを探す
	for (std::size_t pageIndex : batch.pageIndices)
	{
		if (pageIndex >= pages.size())
		{
			DEBUG_LOG_ERROR("Batchが保持するPage番号が不正です\n");
			return UploadResult::OtherFailed;
		}

		UploadPage& page{ pages[pageIndex] };

		if (!page.inUse || !page.resource || !page.mappedData || page.usedBytes > page.capacity)
		{
			DEBUG_LOG_ERROR("使用中Pageの管理情報が不正です\n");
			return UploadResult::OtherFailed;
		}

		// 現在の末尾が配置境界から何Byteずれているか
		const UINT64 remainder{ page.usedBytes % _alignment };

		// 次の境界までの必要な間
		const UINT64 padding{ remainder == 0 ? 0 : _alignment - remainder };

		// 間だけで容量を超えるならこのPageには置けない
		if (padding > page.capacity - page.usedBytes) continue;

		// 収まることが確認できたので追加
		const UINT64 offset{ page.usedBytes + padding };

		// データ本体が残り容量に収まるか確認する
		if (_sizeInBytes > page.capacity - offset) continue;

		// 隙間も含めて利用範囲を進める
		page.usedBytes = offset + _sizeInBytes;

		_outAllocation = UploadAllocation{ pageIndex, offset, _sizeInBytes };
		return UploadResult::Success;
	}

	// このbatchのPageには入らなかったので未使用を探す
	std::size_t availablePageIndex{ INVALID_UPLOAD_INDEX };
	for (std::size_t i = 0; i < pages.size(); i++)
	{
		UploadPage& page{ pages[i] };

		// 別のbatchが使用しているPageには触らない
		if (page.inUse) continue;
		if (!page.resource) continue; // リソースが空であることで解放済みと考える

		if (!page.mappedData || page.usedBytes != 0)
		{
			DEBUG_LOG_ERROR("未使用Pageの管理情報が不正です\n");
			return UploadResult::OtherFailed;
		}

		if (page.capacity < _sizeInBytes) continue;

		availablePageIndex = i;
		break;
	}

	// page番号を追加できるかResource作成前に確認
	if (batch.pageIndices.size() >= batch.pageIndices.max_size())
	{
		DEBUG_LOG_ERROR("Batchが保持できるPage数の上限に達しました\n");
		return UploadResult::OtherFailed;
	}

	// 再利用できるPageがなければ新しく作成する
	if (availablePageIndex == INVALID_UPLOAD_INDEX)
	{
		UploadResult result{ CreateUploadPage(_sizeInBytes, availablePageIndex) };
		if (result != UploadResult::Success) return result;
	}

	try
	{
		// 使用する前にBatchへページ番号へ登録する
		batch.pageIndices.emplace_back(availablePageIndex);
	}
	catch (const std::bad_alloc&)
	{
		DEBUG_LOG_ERROR("BatchへのPage番号の登録に失敗しました\n");
		return UploadResult::OtherFailed;
	}

	// CreateUploadPageでpagesが再確保されてもいいように作成後に参照を取得
	UploadPage& page{ pages[availablePageIndex] };
	page.inUse = true;

	// 未使用なので先頭から確保
	page.usedBytes = _sizeInBytes;
	_outAllocation = UploadAllocation{ availablePageIndex, 0, _sizeInBytes };
	return UploadResult::Success;
}

UploadResult  UploadSystem::CreateUploadPage(UINT64 _minimumSize, std::size_t& _outPageIndex)
{
	// 失敗した際の補償
	_outPageIndex = INVALID_UPLOAD_INDEX;

	if (!initialized || !device)
	{
		DEBUG_LOG_ERROR("UploadSystemが初期化されていません\n");
		return UploadResult::OtherFailed;
	}

	if (_minimumSize == 0)
	{
		DEBUG_LOG_ERROR("必要なPage容量が0です\n");
		return UploadResult::OtherFailed;
	}

	// 通常は基本サイズ、それを超える要求なら必要サイズ
	const UINT64 capacity{ _minimumSize > pageSize ? _minimumSize : pageSize };

	// 確保できる最大容量をチェックしてから残量が確保容量を超えていないか確認することでUINTの上限を超えて巻き戻らないようにする

	// 再計算後で値が変動するので通常の変数ではなくラムダに保存しておく
	const auto& isWithinCapacity
	{
		[&]() { return allocatedUploadBytes <= maxUploadBytes && capacity <= maxUploadBytes - allocatedUploadBytes; }
	};
	if (!isWithinCapacity())
	{
		// inUseがfalseのページを全て解放する
		for (auto& page : pages)
		{
			if (page.inUse) continue;
			if (page.resource && page.mappedData) page.resource->Unmap(0, nullptr); // 0はサブリソース番号
			page.mappedData = nullptr;
			page.resource.Reset();
			allocatedUploadBytes -= page.capacity; // 使用する容量をなくす
			page.capacity = 0;
			page.usedBytes = 0;
		}

		// 使われていないものを解放した状態でもう一度容量ないか確かめる
		if (!isWithinCapacity())
		{
			DEBUG_LOG_ERROR("転送用メモリの確保上限を超えます\n"); // それでもだめなら超える
			return UploadResult::OverBudget;
		}
	}

	// 途中で失敗しても影響がないようにローカルで作る
	UploadPage page{};

	// CPUから書き込みGPUが読み込むメモリ領域
	const CD3DX12_HEAP_PROPERTIES heapProperties{ D3D12_HEAP_TYPE_UPLOAD };

	// capacityByte分のBufferを作るための情報構築
	const CD3DX12_RESOURCE_DESC resourceDesc{ CD3DX12_RESOURCE_DESC::Buffer(capacity) };

	// リソースの作成
	HRESULT result{ device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ, // uploadに必要な初期状態
		nullptr,
		IID_PPV_ARGS(page.resource.GetAddressOf()) // 作成結果の書き戻し先
		) };
	DEBUG_ASSERT(SUCCEEDED(result) && "正しくリソースが作成されませんでした\n");
	if (FAILED(result)) return UploadResult::OtherFailed; // Releaseでも受けられるように止める

	// CPUからアクセスできるようにするためのアドレスを受け取る
	void* mappedPtr{ nullptr };

	// CPUから読み取らずに書き込みだけを行う
	const D3D12_RANGE readRange{ 0, 0 };

	// 実際にMapを行う
	result = page.resource->Map(0, &readRange, &mappedPtr);
	DEBUG_ASSERT(SUCCEEDED(result) && "正しくMapできませんでした\n");
	if (FAILED(result) || !mappedPtr) return UploadResult::OtherFailed; // Releaseでも受けられるように止める

	// バイト単位で位置を計算できるポインタとして保持する
	page.mappedData = static_cast<std::byte*>(mappedPtr);
	page.capacity = capacity;

	// リソースが空 = すでに解放されているページがあればそこに入れる
	std::size_t emptyIndex{ pages.size() };
	for (std::size_t i = 0; i < pages.size(); i++)
	{
		if (!pages[i].resource)
		{
			emptyIndex = i;
			break;
		}
	}
	// 空きIndexが最後尾でなければそこにいれる
	if (emptyIndex < pages.size())
	{
		pages[emptyIndex] = std::move(page); // 空いている場所に所有権を移す
		allocatedUploadBytes += capacity;
		_outPageIndex = emptyIndex;
		return UploadResult::Success;
	}

	try
	{
		// Mapやリソースの参照先を持った状態で実体配列へ
		pages.emplace_back(std::move(page));
	}
	catch (const std::bad_alloc&) // 例外を出したオブジェクトを参照で受け取る
	{
		DEBUG_LOG_ERROR("UploadPageの管理配列のメモリ確保に失敗しました\n");
		return UploadResult::OtherFailed;
	}

	// 登録が成功したので管理情報を確定する
	allocatedUploadBytes += capacity;
	_outPageIndex = pages.size() - 1; // 最後尾に入っているため

	return UploadResult::Success;
}

void UploadSystem::RecycleBatch(std::size_t _batchIndex)
{
	if (!initialized || _batchIndex >= batches.size())
	{
		DEBUG_LOG_ERROR("回収するBatchの番号または初期化状態が不正です\n");
		return;
	}

	UploadBatch& batch{ batches[_batchIndex] };
	if (batch.state != BatchState::InFlight || batch.fenceValue == 0)
	{
		DEBUG_LOG_ERROR("回収するBatchの状態が不正です\n");
		return;
	}

	// 途中で不正なPageが存在したときに中途半端に結果が残らないように先に全てチェックする
	for (std::size_t pageIndex : batch.pageIndices)
	{
		if (pageIndex >= pages.size())
		{
			DEBUG_LOG_ERROR("回収するPageの番号が不正です\n");
			return;
		}

		const UploadPage& page{ pages[pageIndex] };

		if (!page.inUse || !page.resource || !page.mappedData || page.usedBytes > page.capacity)
		{
			DEBUG_LOG_ERROR("回収するPageの管理情報が不正です\n");
			return;
		}
	}

	// このBatchが使っていたPageを次の転送に使える状態へ戻す
	for (std::size_t pageIndex : batch.pageIndices)
	{
		UploadPage& page{ pages[pageIndex] };

		page.usedBytes = 0;
		page.inUse = false;
	}

	// UploadSystemが保持している転送先Resourceの参照を離す
	batch.requests.clear();

	// 前の転送情報をを消す
	batch.bufferCopies.clear();
	batch.textureCopies.clear();
	batch.pageIndices.clear();

	batch.fenceValue = 0;

	batch.state = BatchState::Free;
}
