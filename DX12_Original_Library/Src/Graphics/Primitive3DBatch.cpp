#include <cstring>
#include <limits>
#include <cstdint>
#include "../Debug/DebugLogs.h"
#include "GraphicsDevice.h"
#include "GraphicsResourceManager.h"
#include "Primitive3DBatch.h"


bool Primitive3DBatch::Setup(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _fillPipeLineState, ID3D12PipelineState* _wirePipeLineState, ID3D12PipelineState* _debugLPipeLineState)
{
	if (!_rootSig || !_fillPipeLineState || !_wirePipeLineState || !_debugLPipeLineState)
	{
		DEBUG_LOG_ERROR("Primitive3DBatchの初期か引数が不正です\n");
		return false;
	}

	// size_tで計算してからDynamicBuffer作成へ　UINTで収まるか確認する
	const std::size_t bufferSize{ sizeof(Primitive3DInstanceData) * MAX_PRIMITIVE_3D_INSTANCE_COUNT };
	if (bufferSize > static_cast<std::size_t>((std::numeric_limits<UINT>::max)()))
	{
		DEBUG_LOG_ERROR("PrimitiveのインスタンスバッファサイズがUINT上限を超えています\n");
		return false;
	}

	rootSignature = _rootSig;
	pipelines[static_cast<std::size_t>(Primitive3DDrawMode::Fill)] = _fillPipeLineState;
	pipelines[static_cast<std::size_t>(Primitive3DDrawMode::MeshWireframe)] = _wirePipeLineState;
	pipelines[static_cast<std::size_t>(Primitive3DDrawMode::DebugLine)] = _debugLPipeLineState;

	// GPUが使用中のフレーム領域をCPUが上書きしないようにバックバッファ数だけ独立して確保
	for (DynamicBuffer& buffer : instanceBuffers)
	{
		buffer = GraphicsResourceManager::Instance().CreateDynamicBuffer(static_cast<UINT>(bufferSize));
		if (!buffer.resource || !buffer.mappedPtr)
		{
			DEBUG_LOG_ERROR("Primitive3DInstanceBuffer作成に失敗しました\n");
			Shutdown();
			return false;
		}
	}

	frameConstantBuffer.Setup(static_cast<UINT>(sizeof(Primitive3DFrameData)), 1);
	if (!CreateCubeMesh() || !CreateSphereMesh() || !CreateCylinderMesh() || !CreateHemisphereMesh())
	{
		Shutdown();
		return false;
	}

	// 初回の頻繁な再確保を抑える 全Bucketへ最大量ずつ確保しないよう小さな初期容量だけ
	for (auto& modeBuckets : buckets)
	{
		for (Primitive3DInstanceBucket& bucket : modeBuckets)
		{
			bucket.instances.reserve(64);
		}
	}
	return true;
}

void Primitive3DBatch::Shutdown()
{
	for (DynamicBuffer& buffer : instanceBuffers)
	{
		buffer = {};
	}

	for (auto& modeBuckets : buckets)
	{
		for (Primitive3DInstanceBucket& bucket : modeBuckets)
		{
			// 終了時なのでcapacityも返却
			bucket.instances = std::vector<Primitive3DInstanceData>{};
		}
	}

	for (auto& modeRanges : ranges)
	{
		for (Primitive3DInstanceRange& range : modeRanges)
		{
			range = {};
		}
	}
	frameConstantBuffer.Shutdown();
	for (Primitive3DMesh& mesh : meshes)
	{
		mesh = {};
	}

	registeredInstanceCount = 0;
	droppedInstanceCount = 0;

	rootSignature = nullptr;
	pipelines.fill(nullptr);
}

bool Primitive3DBatch::Register(Primitive3DMeshID _meshID, const Primitive3DInstanceData& _instance, Primitive3DDrawMode _drawMode)
{
	// 単体登録も1要素のグループとして扱い容量・ID検査の実装をRegisterGroupへ一本化する
	const std::array<Primitive3DRegistration, 1> registrations
	{
		Primitive3DRegistration
		{
			_meshID,
			_instance,
			_drawMode
		}
	};

	return RegisterGroup(registrations);
}

void Primitive3DBatch::Reset()
{
	if (droppedInstanceCount > 0) DEBUG_LOG_WARNING("Primitive3Dの登録上限を超えたため{}個を破棄しました\n", droppedInstanceCount);
	for (auto& modeBuckets : buckets)
	{
		for (Primitive3DInstanceBucket& bucket : modeBuckets)
		{
			//  capacityは残して次フレームで再利用
			bucket.instances.clear();
		}
	}

	for (auto& modeRanges : ranges)
	{
		for (Primitive3DInstanceRange& range : modeRanges)
		{
			range = {};
		}
	}
	frameConstantBuffer.Reset();
	registeredInstanceCount = 0;
	droppedInstanceCount = 0;
}

bool Primitive3DBatch::RegisterGroup(std::span<const Primitive3DRegistration> _registration)
{
	if (_registration.empty()) return true; // 空なのでtrueを返す

	// registeredInstanceCountが上限を超えていると残り容量の引き算でアンダーフローするため先に検査する
	if (registeredInstanceCount > MAX_PRIMITIVE_3D_INSTANCE_COUNT)
	{
		// Assertで止める前に値を出す
		DEBUG_LOG_ERROR("Primitive3Dの登録数が上限を超えた不正な値です registered : {} max : {}", registeredInstanceCount, MAX_PRIMITIVE_3D_INSTANCE_COUNT);
		DEBUG_ASSERT(false); // registeredInstanceCountは上限以下でないといけないので止める
		return false;
	}

	// 現在後何個登録できるか
	const std::size_t remainingCapacity{ MAX_PRIMITIVE_3D_INSTANCE_COUNT - registeredInstanceCount };
	// Group全体が追加できるか確認
	if (_registration.size() > remainingCapacity)
	{
		DEBUG_LOG_ERROR("残り容量を超えたグループを登録しようとしました 残り容量 : {} 登録量 : {}\n", remainingCapacity, _registration.size());
		droppedInstanceCount += _registration.size(); // 全ての要素を破棄するため全要素を加える
		return false;
	}

	for (const Primitive3DRegistration& registration : _registration)
	{
		// ID確認
		const std::size_t meshIndex{ static_cast<std::size_t>(registration.meshID) };
		if (meshIndex >= MESH_COUNT)
		{
			DEBUG_LOG_ERROR("RegisterGroupに無効なMeshIDが含まれています\n");
			return false;
		}
		// DrawMode確認
		const std::size_t modeIndex{ static_cast<std::size_t>(registration.drawMode) };
		if (modeIndex >= DRAW_MODE_COUNT)
		{
			DEBUG_LOG_ERROR("RegisterGroupに無効なDrawModeが含まれています\n");
			return false;
		}
	}

	// 確認ができたので全てのデータをpushbackする
	for (const Primitive3DRegistration& registration : _registration)
	{
		// Registerを使うと確認が重複するので自前で追加
		const std::size_t meshIndex{ static_cast<std::size_t>(registration.meshID) };
		const std::size_t modeIndex{ static_cast<std::size_t>(registration.drawMode) };
		buckets[modeIndex][meshIndex].instances.push_back(registration.instance);
	}
	// 全要素の登録が完了したので件数の反映
	registeredInstanceCount += _registration.size();
	return true;
}

bool Primitive3DBatch::Flush(const Mat4x4& _viewProjection)
{
	if (registeredInstanceCount == 0) return true; // 登録されていない時は計算しない
	if (!UploadCurrentFrameInstances()) return false; // 失敗したときは関数内でログが出る

	Primitive3DFrameData frameData{};
	frameData.viewProjection = _viewProjection;

	const D3D12_GPU_VIRTUAL_ADDRESS frameAddress{ frameConstantBuffer.Update(&frameData, static_cast<UINT>(sizeof(frameData))) };
	if (frameAddress == 0)
	{
		DEBUG_LOG_ERROR("Primitive3DのViewProjectionの更新に失敗しました\n");
		return false;
	}

	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	const DynamicBuffer& instanceBuffer{ instanceBuffers[frameIndex] };

	ID3D12GraphicsCommandList* commandList{ GraphicsDevice::Instance().GetCommandList() };
	if (!commandList || !instanceBuffer.resource || !rootSignature)
	{
		DEBUG_LOG_ERROR("Primitive3Dの描画に必要な状態が無効です\n");
		return false;
	}

	commandList->SetGraphicsRootSignature(rootSignature);

	// RootPrameter[0] = b0
	commandList->SetGraphicsRootConstantBufferView(0, frameAddress);

	const D3D12_GPU_VIRTUAL_ADDRESS instanceBufferBase{ instanceBuffer.resource->GetGPUVirtualAddress() };

	for (std::size_t modeIndex = 0; modeIndex < DRAW_MODE_COUNT; modeIndex++)
	{
		ID3D12PipelineState* pipeline{ pipelines[modeIndex] };
		if (!pipeline)
		{
			DEBUG_LOG_ERROR("Primitive3DのPipelineが無効です DrawMode : {}\n", modeIndex);
			continue;
		}
		commandList->SetPipelineState(pipeline);
		const Primitive3DDrawMode drawMode{ static_cast<Primitive3DDrawMode>(modeIndex) };

		// DebugLineだけ線分として解釈する
		const D3D_PRIMITIVE_TOPOLOGY primitiveTopology{ drawMode == Primitive3DDrawMode::DebugLine ? D3D_PRIMITIVE_TOPOLOGY_LINELIST : D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST };
		commandList->IASetPrimitiveTopology(primitiveTopology);

		for (std::size_t meshIndex = 0; meshIndex < MESH_COUNT; meshIndex++)
		{
			const Primitive3DInstanceRange& range{ ranges[modeIndex][meshIndex] };
			if (range.instanceCount == 0) continue; // インスタンスがないならスキップ

			const Primitive3DMesh& mesh{ meshes[meshIndex] };
			const bool isDebugLine{ drawMode == Primitive3DDrawMode::DebugLine };
			const IndexBuffer& drawIndexBuffer{isDebugLine ? mesh.debugLineIndexBuffer : mesh.triangleIndexBuffer}
;			if (!mesh.vertexBuffer.resource || !drawIndexBuffer.resource || drawIndexBuffer.indexCount == 0)
			{
				DEBUG_LOG_ERROR("登録されたPrimitive3DMeshが作成されていません MeshID : {} DrawMode : {}", meshIndex, modeIndex);
				continue;
			}
			commandList->IASetVertexBuffers(0, 1, &mesh.vertexBuffer.vertexView);
			commandList->IASetIndexBuffer(&drawIndexBuffer.indexView);

			// このBacketの先頭までRootSRVのアドレスを進めるのでVS側のSV_InstanceIDは0始まりで使用できる
			const D3D12_GPU_VIRTUAL_ADDRESS bucketAddress{ instanceBufferBase + static_cast<UINT64>(range.startInstance) * sizeof(Primitive3DInstanceData) };
		
			// RootParameter[1] = t0
			commandList->SetGraphicsRootShaderResourceView(1, bucketAddress);
			commandList->DrawIndexedInstanced(drawIndexBuffer.indexCount, range.instanceCount, 0, 0, 0);
		}
	}
	return true;
}

bool Primitive3DBatch::UploadCurrentFrameInstances()
{
	if (registeredInstanceCount == 0) return true;
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };

	DynamicBuffer& currentBuffer{ instanceBuffers[frameIndex] };
	if (!currentBuffer.mappedPtr || !currentBuffer.resource)
	{
		DEBUG_LOG_ERROR("Primitive3Dの現在フレーム用バッファが無効です\n");
		return false;
	}

	auto* destination{ static_cast<Primitive3DInstanceData*>(currentBuffer.mappedPtr) };

	UINT writePosition{ 0 };
	// 各個体を全探索する
	for (std::size_t modeIndex = 0; modeIndex < DRAW_MODE_COUNT; modeIndex++)
	{
		for (std::size_t meshIndex = 0; meshIndex < MESH_COUNT; meshIndex++)
		{
			const auto& instances{ buckets[modeIndex][meshIndex].instances }; // モードとメッシュが同一の配列を取り出す
			Primitive3DInstanceRange& range{ ranges[modeIndex][meshIndex] };
			range.startInstance = writePosition;
			range.instanceCount = static_cast<UINT>(instances.size());
			if (instances.empty()) continue; // 空の場合は計算しない

			const std::size_t copySize{ instances.size() * sizeof(Primitive3DInstanceData) };
			std::memcpy(destination + writePosition, instances.data(), copySize);
			writePosition += range.instanceCount;
		}
	}
	// Register時の合計と実際にコピーした合計が一致するか検証
	DEBUG_ASSERT(writePosition == registeredInstanceCount);
	return writePosition == registeredInstanceCount;
}

bool Primitive3DBatch::CreateCubeMesh()
{
	constexpr float HALF{ 0.5f };
	// 面ごとに独立した4頂点を持たせることで各面二平らな法線を設定する
	constexpr std::array<Primitive3DVertex, 24> VERTICES
	{
		// 前面 -Z
		Primitive3DVertex{{-HALF, -HALF, -HALF}, {0.0f, 0.0f, -1.0f}},
		Primitive3DVertex{{-HALF,  HALF, -HALF}, { 0.0f,  0.0f, -1.0f}},
		Primitive3DVertex{{ HALF,  HALF, -HALF}, { 0.0f,  0.0f, -1.0f}},
		Primitive3DVertex{{ HALF, -HALF, -HALF}, { 0.0f,  0.0f, -1.0f}},

		// 後面 +Z
		Primitive3DVertex{{ HALF, -HALF,  HALF}, { 0.0f,  0.0f,  1.0f}},
		Primitive3DVertex{{ HALF,  HALF,  HALF}, { 0.0f,  0.0f,  1.0f}},
		Primitive3DVertex{{-HALF,  HALF,  HALF}, { 0.0f,  0.0f,  1.0f}},
		Primitive3DVertex{{-HALF, -HALF,  HALF}, { 0.0f,  0.0f,  1.0f}},

		// 左面 -X
		Primitive3DVertex{{-HALF, -HALF,  HALF}, {-1.0f,  0.0f,  0.0f}},
		Primitive3DVertex{{-HALF,  HALF,  HALF}, {-1.0f,  0.0f,  0.0f}},
		Primitive3DVertex{{-HALF,  HALF, -HALF}, {-1.0f,  0.0f,  0.0f}},
		Primitive3DVertex{{-HALF, -HALF, -HALF}, {-1.0f,  0.0f,  0.0f}},

		// 右面 +X
		Primitive3DVertex{{ HALF, -HALF, -HALF}, { 1.0f,  0.0f,  0.0f}},
		Primitive3DVertex{{ HALF,  HALF, -HALF}, { 1.0f,  0.0f,  0.0f}},
		Primitive3DVertex{{ HALF,  HALF,  HALF}, { 1.0f,  0.0f,  0.0f}},
		Primitive3DVertex{{ HALF, -HALF,  HALF}, { 1.0f,  0.0f,  0.0f}},

		// 上面 +Y
		Primitive3DVertex{{-HALF,  HALF, -HALF}, { 0.0f,  1.0f,  0.0f}},
		Primitive3DVertex{{-HALF,  HALF,  HALF}, { 0.0f,  1.0f,  0.0f}},
		Primitive3DVertex{{ HALF,  HALF,  HALF}, { 0.0f,  1.0f,  0.0f}},
		Primitive3DVertex{{ HALF,  HALF, -HALF}, { 0.0f,  1.0f,  0.0f}},

		// 下面 -Y
		Primitive3DVertex{{-HALF, -HALF,  HALF}, { 0.0f, -1.0f,  0.0f}},
		Primitive3DVertex{{-HALF, -HALF, -HALF}, { 0.0f, -1.0f,  0.0f}},
		Primitive3DVertex{{ HALF, -HALF, -HALF}, { 0.0f, -1.0f,  0.0f}},
		Primitive3DVertex{{ HALF, -HALF,  HALF}, { 0.0f, -1.0f,  0.0f}}
	};

	constexpr std::array<std::uint32_t, 36> INDECES
	{
		0, 1, 2, 0, 2, 3,
		4, 5, 6, 4, 6, 7,
		8, 9, 10, 8, 10, 11,
		12, 13, 14, 12, 14, 15,
		16, 17, 18, 16, 18, 19,
		20, 21, 22, 20, 22, 23
	};

	constexpr std::array<std::uint32_t, 24> DEBUG_LINE_INDICES
	{
		0, 1, 1, 2, 2, 3, 3, 0, // zマイナス側の四角形
		7, 6, 6, 5, 5, 4, 4, 7, // zプラス側
		0, 7, 1, 6, 2, 5, 3, 4, // 前後を結ぶ4辺
	};

	const std::size_t meshIndex{ static_cast<std::size_t>(Primitive3DMeshID::Cube) };
	Primitive3DMesh& cube{ meshes[meshIndex] };

	cube.vertexBuffer = GraphicsResourceManager::Instance().CreateVertexBuffer(VERTICES.data(), static_cast<UINT>(sizeof(VERTICES)), static_cast<UINT>(sizeof(Primitive3DVertex)));
	cube.triangleIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(INDECES.data(), static_cast<UINT>(sizeof(INDECES)), static_cast<UINT>(INDECES.size()));
	cube.debugLineIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(DEBUG_LINE_INDICES.data(), static_cast<UINT>(sizeof(DEBUG_LINE_INDICES)), static_cast<UINT>(DEBUG_LINE_INDICES.size()));
	if (!cube.HasTriangleGeometry() || !cube.HasDebugLineGeometry())
	{
		DEBUG_LOG_ERROR("Primitive3Dの単位Cube作成に失敗しました\n");
		return false;
	}
	return true;
}

bool Primitive3DBatch::CreateSphereMesh()
{
	constexpr float radius{ 0.5f }; // 単位なので半径は0.5
	std::vector<Primitive3DVertex> vertices{};
	std::vector<std::uint32_t> triangleIndices{};
	std::vector<std::uint32_t> debugLineIndices{};

	// 赤道を正確に選ぶため、縦分割数は偶数が必要
	static_assert(PRIMITIVE_3D_SPHERE_STACK_COUNT % 2 == 0, "SphereのStack数は偶数にしてください");
	// 0、90、180、270度の経線を選ぶため4の倍数が必要
	static_assert(PRIMITIVE_3D_SPHERE_SLICE_COUNT % 4 == 0, "SphereのSlice数は4の倍数にしてください");

	// UVの継ぎ目では同じ位置を2回持つため+1
	vertices.reserve(static_cast<std::size_t>(PRIMITIVE_3D_SPHERE_STACK_COUNT + 1) * static_cast<std::size_t>(PRIMITIVE_3D_SPHERE_SLICE_COUNT + 1));

	for (UINT stack = 0; stack <= PRIMITIVE_3D_SPHERE_STACK_COUNT; stack++)
	{
		// 0-πで上端から下端へ進む
		const float phi{ Math::PI * static_cast<float>(stack) / static_cast<float>(PRIMITIVE_3D_SPHERE_STACK_COUNT) };

		const float y{ std::cos(phi) };
		const float ringRadius{ std::sin(phi) };

		for (UINT slice = 0; slice <= PRIMITIVE_3D_SPHERE_SLICE_COUNT; slice++)
		{
			// 0-2πで球体を一周
			const float theta{ 2.0f * Math::PI * static_cast<float>(slice) / static_cast<float>(PRIMITIVE_3D_SPHERE_SLICE_COUNT) };

			// 半径1の方向ベクトル
			const Vector3 normal{ ringRadius * std::cos(theta), y, ringRadius * std::sin(theta) };
			const Vector3 position{ normal * radius };

			vertices.push_back(Primitive3DVertex{ {position.x, position.y, position.z}, {normal.x, normal.y, normal.z} });
		}
	}

	// 上下に隣接する頂点を結んで三角形を作る
	for (UINT stack = 0; stack < PRIMITIVE_3D_SPHERE_STACK_COUNT; stack++)
	{
		for (UINT slice = 0; slice < PRIMITIVE_3D_SPHERE_SLICE_COUNT; slice++)
		{
			const std::uint32_t topLeft{ stack * (PRIMITIVE_3D_SPHERE_SLICE_COUNT + 1) + slice };
			const std::uint32_t topRight{ topLeft + 1 };
			const std::uint32_t bottomLeft{ (stack + 1) * (PRIMITIVE_3D_SPHERE_SLICE_COUNT + 1) + slice };
			const std::uint32_t bottomRight{ bottomLeft + 1 };

			// 最上段では同じ位置の頂点を結び退化三角形を作らない
			if (stack != 0)
			{
				triangleIndices.push_back(topLeft);
				triangleIndices.push_back(bottomLeft);
				triangleIndices.push_back(topRight);
			}

			if (stack != PRIMITIVE_3D_SPHERE_STACK_COUNT - 1)
			{
				triangleIndices.push_back(topRight);
				triangleIndices.push_back(bottomLeft);
				triangleIndices.push_back(bottomRight);
			}
		}
	}

	// stackとsliceから既存Sphereの頂点番号を求める
	const auto GetVertexIndex = [](UINT _stack, UINT _slice) -> std::uint32_t
		{
			return static_cast<std::uint32_t>(_stack * (PRIMITIVE_3D_SPHERE_SLICE_COUNT + 1) + _slice);
		};

	// 2頂点を1本の線にして登録
	const auto AddLine = [&debugLineIndices](std::uint32_t _start, std::uint32_t _end)
		{
			debugLineIndices.push_back(_start);
			debugLineIndices.push_back(_end);
		};

	// XZ平面の赤道
	const UINT equatorStack{ PRIMITIVE_3D_SPHERE_STACK_COUNT / 2 };
	for (UINT slice = 0; slice < PRIMITIVE_3D_SPHERE_SLICE_COUNT; slice++)
	{
		AddLine(GetVertexIndex(equatorStack, slice), GetVertexIndex(equatorStack, slice + 1));
	}

	// 縦方向2つの円を求める
	// 使用する経度
	constexpr std::array<UINT, 4> MERIDIAN_SLICE
	{
		0, // 0度
		PRIMITIVE_3D_SPHERE_SLICE_COUNT / 4, // 90度
		PRIMITIVE_3D_SPHERE_SLICE_COUNT / 2, // 180度
		PRIMITIVE_3D_SPHERE_SLICE_COUNT * 3 / 4 // 270度
	};
	for (UINT slice : MERIDIAN_SLICE)
	{
		for (UINT stack = 0; stack < PRIMITIVE_3D_SPHERE_STACK_COUNT; stack++)
		{
			AddLine(GetVertexIndex(stack, slice), GetVertexIndex(stack + 1, slice));
		}
	}

	const std::size_t meshIndex{ static_cast<std::size_t>(Primitive3DMeshID::Sphere) };
	Primitive3DMesh& sphere{ meshes[meshIndex] };
	const std::size_t vertexBufferSize{ vertices.size() * sizeof(Primitive3DVertex) };
	const std::size_t triangleIndexBufferSize{ triangleIndices.size() * sizeof(std::uint32_t) };
	const std::size_t debugLineIndexBufferSize{ debugLineIndices.size() * sizeof(std::uint32_t) };

	// UINT -> size_tの縮小変換なので上限確認を入れる
	const bool isVertexBufferSizeValid{ vertexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	const bool isTriangleIndexBufferSizeValid{ triangleIndexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	const bool isDebugLineIndexBufferSizeValid{ debugLineIndexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	if (!isVertexBufferSizeValid || !isTriangleIndexBufferSizeValid || !isDebugLineIndexBufferSizeValid)
	{
		DEBUG_LOG_ERROR("Primitive3D SphereのメッシュサイズがUINT上限を超えています\n");
		return false;
	}

	sphere.vertexBuffer = GraphicsResourceManager::Instance().CreateVertexBuffer(vertices.data(), static_cast<UINT>(vertexBufferSize), static_cast<UINT>(sizeof(Primitive3DVertex)));
	sphere.triangleIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(triangleIndices.data(), static_cast<UINT>(triangleIndexBufferSize), static_cast<UINT>(triangleIndices.size()));
	sphere.debugLineIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(debugLineIndices.data(), static_cast<UINT>(debugLineIndexBufferSize), static_cast<UINT>(debugLineIndices.size()));
	if (!sphere.HasTriangleGeometry() || !sphere.HasDebugLineGeometry())
	{
		DEBUG_LOG_ERROR("Primitive3Dの単位Sphere作成に失敗しました\n");
		return false;
	}
	return true;
}

bool Primitive3DBatch::CreateCylinderMesh()
{
	constexpr float RADIUS{ 0.5f };
	constexpr float HALF_HEIGHT{ 0.5f };

	std::vector<Primitive3DVertex> vertices{};
	std::vector<std::uint32_t> triangleIndices{};
	std::vector<std::uint32_t> debugLineIndices{};

	// 縦線を90度間隔で結ぶため
	static_assert(PRIMITIVE_3D_CYLINDER_DIVISION % 4 == 0, "Cylinderの分割数は4の倍数にしてください");

	// 側面計算
	const std::uint32_t sideStart{ static_cast<std::uint32_t>(vertices.size()) };

	// 開始点と終了点を別頂点にしてインデックス計算を単純化するため +1個作成
	for (UINT i = 0; i <= PRIMITIVE_3D_CYLINDER_DIVISION; i++)
	{
		const float angle{ 2.0f * Math::PI * static_cast<float>(i) / static_cast<float>(PRIMITIVE_3D_CYLINDER_DIVISION) };
		const float x{ std::cos(angle) * RADIUS };
		const float z{ std::sin(angle) * RADIUS };

		// 側面法線には半径を含めずに長さ1の方向を使う
		const float normalX{ std::cos(angle) };
		const float normalZ{ std::sin(angle) };

		vertices.push_back(Primitive3DVertex{ {x, -HALF_HEIGHT, z}, {normalX, 0.0f, normalZ} }); // 下側
		vertices.push_back(Primitive3DVertex{ {x, HALF_HEIGHT, z}, {normalX, 0.0f, normalZ} }); // 上側
	}

	for (UINT i = 0; i < PRIMITIVE_3D_CYLINDER_DIVISION; i++)
	{
		const std::uint32_t bottom0{ sideStart + i * 2 };
		const std::uint32_t top0{ bottom0 + 1 };
		const std::uint32_t bottom1{ sideStart + (i + 1) * 2 };
		const std::uint32_t top1{ bottom1 + 1 };

		// 側面の四角形を2三角形へ分割
		triangleIndices.push_back(bottom0);
		triangleIndices.push_back(top0);
		triangleIndices.push_back(top1);

		triangleIndices.push_back(bottom0);
		triangleIndices.push_back(top1);
		triangleIndices.push_back(bottom1);
	}

	const auto AddLine = [&debugLineIndices](std::uint32_t _start, std::uint32_t _end)
		{
			debugLineIndices.push_back(_start);
			debugLineIndices.push_back(_end);
		};

	// デバッグ用のときの縦線4本
	constexpr std::array<UINT, 4> VERTICAL_LINE_DIVISIONS
	{
		0, // 0度
		PRIMITIVE_3D_CYLINDER_DIVISION / 4, // 90度
		PRIMITIVE_3D_CYLINDER_DIVISION / 2,  // 180度
		PRIMITIVE_3D_CYLINDER_DIVISION * 3 / 4, // 270度
	};

	for (UINT division : VERTICAL_LINE_DIVISIONS)
	{
		const std::uint32_t bottom{ sideStart + division * 2 };
		const std::uint32_t top{ bottom + 1 };
		AddLine(bottom, top);
	}


	// 上蓋
	const std::uint32_t topCenter{ static_cast<std::uint32_t>(vertices.size()) };
	vertices.push_back(Primitive3DVertex{ {0.0f, HALF_HEIGHT, 0.0f}, {0.0f, 1.0f, 0.0f} });

	const std::uint32_t topRimStart{ static_cast<std::uint32_t>(vertices.size()) };
	for (UINT i = 0; i <= PRIMITIVE_3D_CYLINDER_DIVISION; i++)
	{
		const float angle{ 2.0f * Math::PI * static_cast<float>(i) / static_cast<float>(PRIMITIVE_3D_CYLINDER_DIVISION) };
		vertices.push_back(Primitive3DVertex{ {std::cos(angle) * RADIUS, HALF_HEIGHT, std::sin(angle) * RADIUS}, {0.0f, 1.0f, 0.0f} });
	}

	for (UINT i = 0; i < PRIMITIVE_3D_CYLINDER_DIVISION; i++)
	{
		triangleIndices.push_back(topCenter);
		triangleIndices.push_back(topRimStart + i + 1);
		triangleIndices.push_back(topRimStart + i);

		const std::uint32_t currentTop{ (sideStart + i * 2) + 1 };
		const std::uint32_t nextTop{ (sideStart + (i + 1) * 2) + 1 };
		AddLine(currentTop, nextTop);
	}

	// 下蓋
	const std::uint32_t bottomCenter{ static_cast<std::uint32_t>(vertices.size()) };
	vertices.push_back(Primitive3DVertex{ {0.0f, -HALF_HEIGHT, 0.0f}, {0.0f, -1.0f, 0.0f} });

	const std::uint32_t bottomRimStart{ static_cast<std::uint32_t>(vertices.size()) };
	for (UINT i = 0; i <= PRIMITIVE_3D_CYLINDER_DIVISION; i++)
	{
		const float angle{ 2.0f * Math::PI * static_cast<float>(i) / static_cast<float>(PRIMITIVE_3D_CYLINDER_DIVISION) };
		vertices.push_back(Primitive3DVertex{ {std::cos(angle) * RADIUS, -HALF_HEIGHT, std::sin(angle) * RADIUS}, {0.0f, -1.0f, 0.0f} });
	}

	for (UINT i = 0; i < PRIMITIVE_3D_CYLINDER_DIVISION; i++)
	{
		triangleIndices.push_back(bottomCenter);
		triangleIndices.push_back(bottomRimStart + i + 1);
		triangleIndices.push_back(bottomRimStart + i);

		const std::uint32_t currentBottom{ (sideStart + i * 2) };
		const std::uint32_t nextBottom{ (sideStart + (i + 1) * 2) };
		AddLine(currentBottom, nextBottom);
	}

	// GPUリソース作成
	const std::size_t vertexBufferSize{ vertices.size() * sizeof(Primitive3DVertex) };
	const std::size_t triangleIndexBufferSize{ triangleIndices.size() * sizeof(std::uint32_t) };
	const std::size_t debugLineIndexBufferSize{ debugLineIndices.size() * sizeof(std::uint32_t) };

	// UINT -> size_tの縮小変換なので上限確認を入れる
	const bool isVertexBufferSizeValid{ vertexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	const bool isTriangleIndexBufferSizeValid{ triangleIndexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	const bool isDebugLineIndexBufferSizeValid{ debugLineIndexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	if (!isVertexBufferSizeValid || !isTriangleIndexBufferSizeValid || !isDebugLineIndexBufferSizeValid)
	{
		DEBUG_LOG_ERROR("Primitive3D CylinderのメッシュサイズがUINT上限を超えています\n");
		return false;
	}

	const std::size_t meshIndex{ static_cast<std::size_t>(Primitive3DMeshID::Cylinder) };
	Primitive3DMesh& cylinder{ meshes[meshIndex] };

	cylinder.vertexBuffer = GraphicsResourceManager::Instance().CreateVertexBuffer(vertices.data(), static_cast<UINT>(vertexBufferSize), static_cast<UINT>(sizeof(Primitive3DVertex)));
	cylinder.triangleIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(triangleIndices.data(), static_cast<UINT>(triangleIndexBufferSize), static_cast<UINT>(triangleIndices.size()));
	cylinder.debugLineIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(debugLineIndices.data(), static_cast<UINT>(debugLineIndexBufferSize), static_cast<UINT>(debugLineIndices.size()));
	if (!cylinder.HasTriangleGeometry() || !cylinder.HasDebugLineGeometry())
	{
		DEBUG_LOG_ERROR("Primitive3Dの単位Cylinder作成に失敗しました\n");
		return false;
	}
	return true;
}

bool Primitive3DBatch::CreateHemisphereMesh()
{
	// 上半球なので0-π / 2まで生成
	constexpr float RADIUS{ 0.5f };
	constexpr UINT HEMISPHERE_STACK_COUNT{ PRIMITIVE_3D_SPHERE_STACK_COUNT / 2 };

	static_assert(PRIMITIVE_3D_SPHERE_STACK_COUNT % 2 == 0, "SphereのStack数は偶数にしてください");
	static_assert(PRIMITIVE_3D_SPHERE_SLICE_COUNT % 4 == 0, "SphereのSlice数は4の倍数にしてください");

	std::vector<Primitive3DVertex> vertices{};
	std::vector<std::uint32_t> triangleIndices{};
	std::vector<std::uint32_t> debugLineIndices{};

	vertices.reserve(static_cast<std::size_t>(HEMISPHERE_STACK_COUNT + 1) * static_cast<std::size_t>(PRIMITIVE_3D_SPHERE_SLICE_COUNT + 1));
	for (UINT stack = 0; stack <= HEMISPHERE_STACK_COUNT; stack++)
	{
		// 0 - π / 2
		const float phi{ (Math::PI * 0.5f) * static_cast<float>(stack) / static_cast<float>(HEMISPHERE_STACK_COUNT) };
		const float y{ std::cos(phi) };
		const float ringRadius{ std::sin(phi) };
		for (UINT slice = 0; slice <= PRIMITIVE_3D_SPHERE_SLICE_COUNT; slice++)
		{
			const float theta{ 2.0f * Math::PI * static_cast<float>(slice) / static_cast<float>(PRIMITIVE_3D_SPHERE_SLICE_COUNT) };

			// 方向はそのまま法線に使える
			const Vector3 normal
			{
				ringRadius * std::cos(theta),
				y,
				ringRadius * std::sin(theta)
			};

			const  Vector3 position{ normal * RADIUS };
			vertices.push_back(Primitive3DVertex{ position.x, position.y, position.z, normal.x, normal.y, normal.z });
		}
	}

	// Triangle用のインデックス
	const auto GetVertexIndex = [](UINT _stack, UINT _slice) -> std::uint32_t
		{
			return static_cast<std::uint32_t>(_stack * (PRIMITIVE_3D_SPHERE_SLICE_COUNT + 1) + _slice);
		};

	for (UINT stack = 0; stack < HEMISPHERE_STACK_COUNT; stack++)
	{
		for (UINT slice = 0; slice < PRIMITIVE_3D_SPHERE_SLICE_COUNT; slice++)
		{
			const std::uint32_t topLeft{ GetVertexIndex(stack, slice) };
			const std::uint32_t topRight{ GetVertexIndex(stack, slice + 1) };
			const std::uint32_t bottomLeft{ GetVertexIndex(stack + 1, slice) };
			const std::uint32_t bottomRight{ GetVertexIndex(stack + 1, slice + 1) };

			// 北極点では同じ位置の頂点を結ばない
			if(stack != 0)
			{
				triangleIndices.push_back(topLeft);
				triangleIndices.push_back(bottomLeft);
				triangleIndices.push_back(topRight);
			}

			triangleIndices.push_back(topRight);
			triangleIndices.push_back(bottomLeft);
			triangleIndices.push_back(bottomRight);
		}
	}

	// DebugLine用の線分作成ヘルパー
	const auto AddLine = [&debugLineIndices](std::uint32_t _start, std::uint32_t _end)
		{
			debugLineIndices.push_back(_start);
			debugLineIndices.push_back(_end);
		};

	// 円をかくときの角度
	constexpr std::array<UINT, 4> ARC_SLICE
	{
		0, // 0度
		PRIMITIVE_3D_SPHERE_SLICE_COUNT / 4, // 90度
		PRIMITIVE_3D_SPHERE_SLICE_COUNT / 2, // 180度
		PRIMITIVE_3D_SPHERE_SLICE_COUNT * 3 / 4 // 270度
	};

	for (UINT slice : ARC_SLICE)
	{
		for (UINT stack = 0; stack < HEMISPHERE_STACK_COUNT; stack++)
		{
			AddLine(GetVertexIndex(stack, slice), GetVertexIndex(stack + 1, slice));
		}
	}

	// GPUリソース作成
	const std::size_t vertexBufferSize{ vertices.size() * sizeof(Primitive3DVertex) };
	const std::size_t triangleIndexBufferSize{ triangleIndices.size() * sizeof(std::uint32_t) };
	const std::size_t debugLineIndexBufferSize{ debugLineIndices.size() * sizeof(std::uint32_t) };

	// UINT -> size_tの縮小変換なので上限確認を入れる
	const bool isVertexBufferSizeValid{ vertexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	const bool isTriangleIndexBufferSizeValid{ triangleIndexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	const bool isDebugLineIndexBufferSizeValid{ debugLineIndexBufferSize <= static_cast<std::size_t>((std::numeric_limits<UINT>::max)()) };
	if (!isVertexBufferSizeValid || !isTriangleIndexBufferSizeValid || !isDebugLineIndexBufferSizeValid)
	{
		DEBUG_LOG_ERROR("Primitive3D HemisphereのメッシュサイズがUINT上限を超えています\n");
		return false;
	}

	const std::size_t meshIndex{ static_cast<std::size_t>(Primitive3DMeshID::Hemisphere) };
	Primitive3DMesh& hemisphere{ meshes[meshIndex] };

	hemisphere.vertexBuffer = GraphicsResourceManager::Instance().CreateVertexBuffer(vertices.data(), static_cast<UINT>(vertexBufferSize), static_cast<UINT>(sizeof(Primitive3DVertex)));
	hemisphere.triangleIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(triangleIndices.data(), static_cast<UINT>(triangleIndexBufferSize), static_cast<UINT>(triangleIndices.size()));
	hemisphere.debugLineIndexBuffer = GraphicsResourceManager::Instance().CreateIndexBuffer(debugLineIndices.data(), static_cast<UINT>(debugLineIndexBufferSize), static_cast<UINT>(debugLineIndices.size()));
	if (!hemisphere.HasTriangleGeometry() || !hemisphere.HasDebugLineGeometry())
	{
		DEBUG_LOG_ERROR("Primitive3Dの単位Hemisphere作成に失敗しました\n");
		return false;
	}
	return true;

}
