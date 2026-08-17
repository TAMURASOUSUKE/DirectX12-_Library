#pragma once
#include <d3d12.h>
#include ""

// 3Dモデルを描画する機能を提供するクラス
class ModelRenderer
{
public:
	void Setup(const ID3D12GraphicsCommandList* _cmd); // 初期化

	void DrawSkinnedModel(const AnimInstanceData& _anim, Transform _transform); // スキンメッシュ付きモデルのロード
	void DrawStaticmModel(ModelHandle _model, const Transform _transform); // 静的モデルの描画

private:

private:
	ID3D12GraphicsCommandList* cmd{}; // 

};
