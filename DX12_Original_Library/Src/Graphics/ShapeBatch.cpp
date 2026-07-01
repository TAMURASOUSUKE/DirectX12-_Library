#include "../Debug/DebugLogs.h"
#include "GraphicsDevice.h"
#include "GraphicsType.h"
#include "GraphicsConstant.h"
#include "ResourceManager.h"
#include "ShapeBatch.h"

void ShapeBatch::Initialize(ID3D12RootSignature* _rootSig, ID3D12PipelineState* _fillState, ID3D12PipelineState* _wireState, ID3D12Resource* _gpuVirtualAddres)
{
	DEBUG_ASSERT(_rootSig != nullptr && _wireState != nullptr && _fillState != nullptr  &&  _gpuVirtualAddres != nullptr);
	if (_rootSig == nullptr || _wireState == nullptr || _fillState == nullptr || _gpuVirtualAddres == nullptr) return;
	// 各パラメータと繋げる
	rootSig = _rootSig;
	wireState = _wireState;
	fillState = _fillState;
	gpuVirtualAddres = _gpuVirtualAddres;

	// 一番多く頂点を取るカプセルの頂点数(3N * 6)を最大数分確保する
	vertBuffer = ResourceManager::Instance().CreateDynamicVertexBuffer(nullptr, MAX_SHAPE_COUNT * (3 * CIRCLE_DIVISION + 6) * sizeof(ShapeVertex), sizeof(ShapeVertex)); // 動的な頂点バッファの作成
}

void ShapeBatch::RegisterBox(Vector2 _leftTop, Vector2 _rightBottom, float _radRotation , Vector4 _color, bool _isWireframe)
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
		Vector2 center{(_rightBottom - _leftTop) / 2.0f }; // 中心
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

	ShapeVertex* vertices{ static_cast<ShapeVertex*>(vertBuffer.mappedPtr) }; // 頂点バッファの中のvoidPtrをShapeVertexのptrに変換
	// カラーを引数から受け取る(今はインデックスを使わないので頂点を直書きしていく)
	if (_isWireframe)
	{
		vertices[shapeVertexCounter + 0] = { {_leftTop.x, _leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左上
		vertices[shapeVertexCounter + 1] = { {rightTop.x, rightTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右上
		vertices[shapeVertexCounter + 2] = { {_rightBottom.x, _rightBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 右下
		vertices[shapeVertexCounter + 3] = { {leftBottom.x, leftBottom.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左下
		vertices[shapeVertexCounter + 4] = { {_leftTop.x, _leftTop.y, 0.0f}, {_color.x, _color.y, _color.z, _color.w} }; // 左上
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

	UINT vertexNum{ (_isWireframe) ? 5u : 6u };
	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != _isWireframe) // wireかどうかで判断する
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({_isWireframe, shapeVertexCounter, vertexNum});
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count += 1u; // 整数昇格による縮小変換防止
	}

	// カウンターを増加する
	shapeCounter++; 
	shapeVertexCounter += vertexNum;
}

void ShapeBatch::RegisterSphere(Vector2 _position, float _radius, Vector4 _color, bool _isWireframe)
{
	if (shapeCounter >= MAX_SHAPE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}


	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != _isWireframe) // wireかどうかで判断する
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({ _isWireframe, shapeCounter, 1 });
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count++;
	}

	shapeCounter++; // カウンターを増加する
}

void ShapeBatch::RegisterCapsule(Vector2 _startPos, Vector2 _endPos, float _radius, float _radRotation, Vector4 _color, bool _isWireframe)
{
	if (shapeCounter >= MAX_SHAPE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}

	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != _isWireframe) // wireかどうかで判断する
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({ _isWireframe, shapeCounter, 1 });
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count++;
	}

	shapeCounter++; // カウンターを増加する
}
void ShapeBatch::RegisterLine(Vector2 _startPos, Vector2 _endPos, Vector4 _color)
{
	if (shapeCounter >= MAX_SHAPE_COUNT)
	{
		droppedCounter++; // あふれているならカウントする
		return; // 限界を超えているならreturn
	}

	// run(描画順)を管理する
	if (runs.empty() || runs.back().isWireframe != true)
	{
		// 配列が空もしくは一番最後のワイヤーフラグが登録しようとしているフラグと異なるなら
		runs.push_back({ true, shapeCounter, 1 });
	}
	else
	{
		// 同一の描画方法ならカウントを増やす
		runs.back().count++;
	}

	shapeCounter++; // カウンターを増加する
}

void ShapeBatch::Flush()
{
	if (runs.empty()) return; // 何もなければパイプライン設定などもせずに即return

	auto* cmd{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリストをキャッシュ

	cmd->SetGraphicsRootSignature(rootSig);
	cmd->SetGraphicsRootConstantBufferView(0, gpuVirtualAddres->GetGPUVirtualAddress());
	cmd->IASetVertexBuffers(0, 1, &vertBuffer.vertexView);
	// ワイヤーフラグでrunを切り替えるのでrunごとに確認する
	for (const ShapeDrawRun& run : runs)
	{
		// wireの場合
		if (run.isWireframe)
		{
			cmd->SetPipelineState(wireState);
			cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_LINESTRIP);
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