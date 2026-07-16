#include "../Debug/DebugLogs.h"
#include "GraphicsDevice.h"
#include "GraphicsType.h"
#include "GraphicsConstant.h"
#include "GraphicsResourceManager.h"
#include "ShapeBatch.h"

void ShapeBatch::Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _fillState, ID3D12PipelineState* _wireState, ID3D12Resource* _gpuVirtualAddres)
{
	DEBUG_ASSERT(_rootSig != nullptr && _wireState != nullptr && _fillState != nullptr && _gpuVirtualAddres != nullptr);
	if (_rootSig == nullptr || _wireState == nullptr || _fillState == nullptr || _gpuVirtualAddres == nullptr) return;
	// 各パラメータと繋げる
	rootSig = _rootSig;
	wireState = _wireState;
	fillState = _fillState;
	gpuVirtualAddres = _gpuVirtualAddres;

	// 一番多く頂点を取るカプセルの頂点数(3N * 6)を最大数分確保する
	for (UINT i = 0; i < FRAME_BUFFER_COUNT; i++)
	{
		vertBuffers[i] = GraphicsResourceManager::Instance().CreateDynamicVertexBuffer(nullptr, MAX_SHAPE_COUNT * (3 * CIRCLE_DIVISION + 6) * sizeof(ShapeVertex), sizeof(ShapeVertex)); // 動的な頂点バッファの作成
	}
}

void ShapeBatch::Shutdown()
{
	for (VertexBuffer& buffer : vertBuffers)
	{
		buffer = VertexBuffer{};
	}
	droppedCounter = 0;
	shapeCounter = 0;
	shapeVertexCounter = 0;
	runs = std::vector<ShapeDrawRun>{}; // vectorの確保容量も返す
	rootSig = nullptr;
	wireState = nullptr;
	fillState = nullptr;
	gpuVirtualAddres = nullptr;
}

void ShapeBatch::RegisterBox(Vector2 _leftTop, Vector2 _rightBottom, float _radRotation, Vector4 _color, bool _isWireframe)
{
	if (shapeCounter >= MAX_SHAPE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}

	// 回転の適用
	Vector2 size{ _rightBottom - _leftTop };
	Vector2 rightTop{ _leftTop.x + size.x, _leftTop.y }; // 右上
	Vector2 leftBottom{ _leftTop.x, _leftTop.y + size.y }; // 左下
	if (_radRotation != 0.0f)
	{
		Vector2 center{ (_rightBottom - _leftTop) / 2.0f }; // 中心
		// 相対座標を適用(中心からの位置)
		_leftTop -= center;
		rightTop -= center;
		_rightBottom -= center;
		leftBottom -= center;

		float c{ std::cosf(_radRotation) }; // cosθ
		float s{ std::sinf(_radRotation) }; // sinθ

		_leftTop = { _leftTop.x * c - _leftTop.y * s, _leftTop.x * s + _leftTop.y * c };
		rightTop = { rightTop.x * c - rightTop.y * s, rightTop.x * s + rightTop.y * c };
		_rightBottom = { _rightBottom.x * c - _rightBottom.y * s, _rightBottom.x * s + _rightBottom.y * c };
		leftBottom = { leftBottom.x * c - leftBottom.y * s, leftBottom.x * s + leftBottom.y * c };

		_leftTop = _leftTop + center;
		rightTop = rightTop + center;
		_rightBottom = _rightBottom + center;
		leftBottom = leftBottom + center;
	}
	// 今描画しているBackBufferと同じ番号の頂点バッファへ書き込む
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	ShapeVertex* vertices{ static_cast<ShapeVertex*>(vertBuffers[frameIndex].mappedPtr)}; // 頂点バッファの中のvoidPtrをShapeVertexのptrに変換
	// カラーを引数から受け取る(今はインデックスを使わないので頂点を直書きしていく)
	if (_isWireframe)
	{
		// LINELISTのため頂点をかぶせるような形にする
		vertices[shapeVertexCounter + 0] = { {_leftTop.x, _leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左上
		vertices[shapeVertexCounter + 1] = { {rightTop.x, rightTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右上
		vertices[shapeVertexCounter + 2] = { {rightTop.x, rightTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右上
		vertices[shapeVertexCounter + 3] = { {_rightBottom.x, _rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右下
		vertices[shapeVertexCounter + 4] = { {_rightBottom.x, _rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右下
		vertices[shapeVertexCounter + 5] = { {leftBottom.x, leftBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左下
		vertices[shapeVertexCounter + 6] = { {leftBottom.x, leftBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左下
		vertices[shapeVertexCounter + 7] = { {_leftTop.x, _leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左上
	}
	else
		// 塗りつぶし
	{
		vertices[shapeVertexCounter + 0] = { {_leftTop.x, _leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左上
		vertices[shapeVertexCounter + 1] = { {rightTop.x, rightTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右上
		vertices[shapeVertexCounter + 2] = { {_rightBottom.x, _rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右下
		vertices[shapeVertexCounter + 3] = { {_leftTop.x, _leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左上
		vertices[shapeVertexCounter + 4] = { {_rightBottom.x, _rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右下
		vertices[shapeVertexCounter + 5] = { {leftBottom.x, leftBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左下
	}

	UINT vertexNum{ (_isWireframe) ? 8u : 6u }; // 整数昇格による縮小変換防止
	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != _isWireframe) // wireかどうかで判断する
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({ _isWireframe, shapeVertexCounter, vertexNum });
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count += vertexNum;
	}

	// カウンターを増加する
	shapeCounter++;
	shapeVertexCounter += vertexNum;
}

void ShapeBatch::RegisterCircle(Vector2 _center, float _radius, Vector4 _color, bool _isWireframe)
{
	if (shapeCounter >= MAX_SHAPE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}
	// 今描画しているBackBufferと同じ番号の頂点バッファへ書き込む
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	ShapeVertex* vertices{ static_cast<ShapeVertex*>(vertBuffers[frameIndex].mappedPtr)}; // 頂点バッファの中のvoidPtrをShapeVertexのptrに変換
	UINT vertexNum{ (_isWireframe) ? 2u * CIRCLE_DIVISION : 3u * CIRCLE_DIVISION }; // 頂点数
	float theta{ (2.0f * Math::PI) / CIRCLE_DIVISION };

	if (_isWireframe)
	{
		// ワイヤーフレーム時
		for (UINT i = 0; i < CIRCLE_DIVISION; i++)
		{
			float angle0{ static_cast<float>(i) * theta }; // 一つ目の頂点の角度
			float angle1{ static_cast<float>(i + 1) * theta }; // 二つ目の頂点の角度

			Vector2 p0{ _center.x + cosf(angle0) * _radius, _center.y + sinf(angle0) * _radius };
			Vector2 p1{ _center.x + cosf(angle1) * _radius, _center.y + sinf(angle1) * _radius };

			UINT base{shapeVertexCounter + i * 2}; // 開始地点 + 現在の番号 * 構成する要素
			vertices[base + 0] = { {p0.x, p0.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 一つ目の頂点
			vertices[base + 1] = { {p1.x, p1.y, 0.0f}, { _color.x, _color.y, _color.z, _color.w } }; // 二つ目の頂点
		}
	}
	else
	{
		// 塗りつぶし
		for (UINT i = 0; i < CIRCLE_DIVISION; i++)
		{
			float angle0{ static_cast<float>(i) * theta }; // 一つ目の頂点の角度
			float angle1{ static_cast<float>(i + 1) * theta }; // 二つ目の頂点の角度

			Vector2 p0{ _center.x + cosf(angle0) * _radius, _center.y + sinf(angle0) * _radius };
			Vector2 p1{ _center.x + cosf(angle1) * _radius, _center.y + sinf(angle1) * _radius };

			UINT base{ shapeVertexCounter + i * 3 }; // 開始地点 + 現在の番号 * 構成する要素
			vertices[base + 0] = { { _center.x, _center.y, 0.0f }, { _color.x, _color.y, _color.z, _color.w } }; // 一つ目の頂点(中心)
			vertices[base + 1] = { {p0.x, p0.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 二つ目の頂点
			vertices[base + 2] = { {p1.x, p1.y, 0.0f}, { _color.x, _color.y, _color.z, _color.w } }; // 三つ目の頂点
		}
	}

	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != _isWireframe) // wireかどうかで判断する
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({ _isWireframe, shapeVertexCounter, vertexNum });
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count += vertexNum; // 整数昇格による縮小変換防止
	}

	// カウンターを増加する
	shapeCounter++;
	shapeVertexCounter += vertexNum;
}

void ShapeBatch::RegisterCapsule(Vector2 _startPos, Vector2 _endPos, float _radius, Vector4 _color, bool _isWireframe)
{
	if (shapeCounter >= MAX_SHAPE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}
	// 今描画しているBackBufferと同じ番号の頂点バッファへ書き込む
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	ShapeVertex* vertices{ static_cast<ShapeVertex*>(vertBuffers[frameIndex].mappedPtr)}; // 頂点バッファの中のvoidPtrをShapeVertexのptrに変換
	Vector2 d{ _endPos - _startPos }; // 始点から終点までのベクトル
	d.Normalize(); // 正規化
	Vector2 normal{ -d.y, d.x }; // 90度回転した方向
	UINT vertexNum{ (_isWireframe) ?( 2u * CIRCLE_DIVISION) + 4 : (3u * CIRCLE_DIVISION) + 6}; // 頂点数
	float theta{ (2.0f * Math::PI) / CIRCLE_DIVISION };
	// 矩形部分の座標
	Vector2 leftTop{_startPos + normal * _radius};
	Vector2 leftBottom{_startPos - normal * _radius};
	Vector2 rightTop{_endPos + normal * _radius};
	Vector2 rightBottom{_endPos - normal * _radius};

	if (_isWireframe)
	{
		UINT base{ shapeVertexCounter }; // 頂点数をキャストする(この後の計算のため)
		// 上辺
		vertices[base + 0] = { {leftTop.x, leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		vertices[base + 1] = { {rightTop.x, rightTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		// 底辺
		vertices[base + 2] = { {leftBottom.x, leftBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		vertices[base + 3] = { {rightBottom.x, rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };

		base += 4u;

		// end側の半円
		for (UINT i = 0; i < CIRCLE_DIVISION / 2u; i++)
		{
			float angle0{ Math::PI * 0.5f - theta * static_cast<float>(i) };
			float angle1{ Math::PI * 0.5f - theta * static_cast<float>(i + 1) };
			Vector2 p0{ _endPos + (d * cosf(angle0) + normal * sinf(angle0)) * _radius };
			Vector2 p1{ _endPos + (d * cosf(angle1) + normal * sinf(angle1)) * _radius };

			UINT lineBase{ base + i * 2u }; // 半円を描くときのoffset
			vertices[lineBase + 0] = { {p0.x, p0.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
			vertices[lineBase + 1] = { {p1.x, p1.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		}

		base += (CIRCLE_DIVISION / 2u) * 2; // 半円の頂点(重なっている頂点を計算しているため * 2)

		// start側の半円
		for (UINT i = 0; i < CIRCLE_DIVISION / 2u; i++)
		{
			float angle0{ -Math::PI * 0.5f - theta * static_cast<float>(i) };
			float angle1{ -Math::PI * 0.5f - theta * static_cast<float>(i + 1) };
			Vector2 p0{ _startPos + (d * cosf(angle0) + normal * sinf(angle0)) * _radius };
			Vector2 p1{ _startPos + (d * cosf(angle1) + normal * sinf(angle1)) * _radius };

			UINT lineBase{ base + i * 2u }; // 半円を描くときのoffset
			vertices[lineBase + 0] = { {p0.x, p0.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
			vertices[lineBase + 1] = { {p1.x, p1.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		}
	}
	else
	{
		UINT base{ shapeVertexCounter }; // 頂点数をキャストする(この後の計算のため)
		// 矩形部分
		vertices[base + 0] = { {leftTop.x, leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		vertices[base + 1] = { {rightTop.x, rightTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		vertices[base + 2] = { {rightBottom.x, rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		vertices[base + 3] = { {leftTop.x, leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		vertices[base + 4] = { {rightBottom.x, rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		vertices[base + 5] = { {leftBottom.x, leftBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };

		base += 6u;

		// end側の半円
		for (UINT i = 0; i < CIRCLE_DIVISION / 2u; i++)
		{
			float angle0{ Math::PI * 0.5f - theta * static_cast<float>(i) };
			float angle1{ Math::PI * 0.5f - theta * static_cast<float>(i + 1) };
			Vector2 p0{ _endPos + (d * cosf(angle0) + normal * sinf(angle0)) * _radius };
			Vector2 p1{ _endPos + (d * cosf(angle1) + normal * sinf(angle1)) * _radius };

			UINT triBase{ base + i * 3u }; // 半円を描くときのoffset
			vertices[triBase + 0] = { {_endPos.x, _endPos.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 中心
			vertices[triBase + 1] = { {p0.x, p0.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
			vertices[triBase + 2] = { {p1.x, p1.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		}

		base += (CIRCLE_DIVISION / 2u) * 3u; // 枚数 * 頂点数;

		// start側の半円
		for (UINT i = 0; i < CIRCLE_DIVISION / 2u; i++)
		{
			float angle0{ -Math::PI * 0.5f - theta * static_cast<float>(i) };
			float angle1{ -Math::PI * 0.5f - theta * static_cast<float>(i + 1) };
			Vector2 p0{ _startPos + (d * cosf(angle0) + normal * sinf(angle0)) * _radius };
			Vector2 p1{ _startPos + (d * cosf(angle1) + normal * sinf(angle1)) * _radius };

			UINT triBase{ base + i * 3u }; // 半円を描くときのoffset
			vertices[triBase + 0] = { {_startPos.x, _startPos.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 中心
			vertices[triBase + 1] = { {p0.x, p0.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
			vertices[triBase + 2] = { {p1.x, p1.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
		}
	}

	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != _isWireframe) // wireかどうかで判断する
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({ _isWireframe, shapeVertexCounter, vertexNum });
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count += vertexNum; // 整数昇格による縮小変換防止
	}

	// カウンターを増加する
	shapeCounter++;
	shapeVertexCounter += vertexNum;
}
void ShapeBatch::RegisterLine(Vector2 _startPos, Vector2 _endPos, Vector4 _color)
{
	if (shapeCounter >= MAX_SHAPE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}
	// 今描画しているBackBufferと同じ番号の頂点バッファへ書き込む
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };
	ShapeVertex* vertices{ static_cast<ShapeVertex*>(vertBuffers[frameIndex].mappedPtr)}; // 頂点バッファの中のvoidPtrをShapeVertexのptrに変換
	UINT vertexNum{ 2u }; // 頂点数

	vertices[shapeVertexCounter + 0] = { {_startPos.x, _startPos.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };
	vertices[shapeVertexCounter + 1] = { {_endPos.x, _endPos.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} };

	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != true)
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({ true, shapeVertexCounter, vertexNum });
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count += vertexNum; // 整数昇格による縮小変換防止
	}

	// カウンターを増加する
	shapeCounter++;
	shapeVertexCounter += vertexNum;
}

void ShapeBatch::Flush()
{
	if (runs.empty()) return; // 何もなければパイプライン設定などもせずに即return

	auto* cmd{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリストをキャッシュ
	// 今描画しているBackBufferと同じ番号の頂点バッファへ書き込む
	const UINT frameIndex{ GraphicsDevice::Instance().GetCurrentFrameIndex() };

	cmd->SetGraphicsRootSignature(rootSig);
	cmd->SetGraphicsRootConstantBufferView(0, gpuVirtualAddres->GetGPUVirtualAddress());
	cmd->IASetVertexBuffers(0, 1, &vertBuffers[frameIndex].vertexView);
	// ワイヤーフラグでrunを切り替えるのでrunごとに確認する
	for (const ShapeDrawRun& run : runs)
	{
		// wireの場合
		if (run.isWireframe)
		{
			cmd->SetPipelineState(wireState);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINELIST);
		}
		// 塗りつぶしの場合
		else
		{
			cmd->SetPipelineState(fillState);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		}

		cmd->DrawInstanced(run.count, 1, run.startVertex, 0); // 区間情報から描画位置を特定して描画する
	}

	runs.clear(); // 消費したのでクリアする
}

void ShapeBatch::Reset()
{
	if (droppedCounter > 0) DEBUG_LOG_WARNING("図形最大描画数を超過しました 超過枚数 : {}", droppedCounter);
	shapeCounter = 0;
	shapeVertexCounter = 0;
	droppedCounter = 0;
	runs.clear(); // Flushで空になるがFlushなしで終わるケースの保険とする
}