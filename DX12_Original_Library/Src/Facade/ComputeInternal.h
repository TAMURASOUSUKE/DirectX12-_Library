#pragma once
#include <vector>

struct ComputeDispatchRequest;

// Computeに関するユーザーから隠蔽するべき機能を作成する
namespace ComputeInternal
{
	// 初期化
	bool Initialize();

	// フレームの開始処理
	void BeginFrame();

	// フレームの終了処理
	void EndFrame();

	// 終了処理
	void Finish();

	// Compute実行要求を登録する
	void RegisterDispatches(const std::vector<ComputeDispatchRequest>& _requests);
}
