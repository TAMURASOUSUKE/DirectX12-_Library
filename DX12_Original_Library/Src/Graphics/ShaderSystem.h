#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;
#pragma comment(lib, "d3d12.lib")

// 内蔵PSOとRootSignatureの台帳と内蔵Shaderのキャッシュ
class ShaderSystem
{
public:
	ShaderSystem() = default; // デフォルトコンストラクタ
	~ShaderSystem() = default; // デフォルトデストラクタ

	void Setup(ID3D12Device* _device); // 初期化処理
	void Shutdown(); // 終了処理

	// Graphics用RootSignatrueを台帳へ記録する
	bool RegisterBuiltinRootSignature(const RootSignatureDesc& _desc);

	// VS,HS,DS,GS,PS用のGraphicsPipelineを生成し台帳へ記録する
	bool RegisterBuiltinPipeline(const GraphicsPipelineDesc& _desc);

	// Material用のPSOを作成する作成したPSOは固定Pipeline台帳には登録せず呼び出し元へ返す(VSがない場合は内蔵を指定する必要があるほか、usageによってrootsigを台帳から引くためSystem内に置く)
	ComPtr<ID3D12PipelineState> CreateMaterialPipeline(ShaderUsage _usage, ID3DBlob* _vertexShader, ID3DBlob* _pixelShader, const MaterialRenderState& _renderState);

	// 汎用RootSignatureDescを作成する関数
	std::vector<RootSignatureDesc> MakeRootSignatureDescs() const;

	// 指定したIDでPipelineを引くことができるGetter
	ID3D12PipelineState* GetPipeline(PipelineID _id) const { return pipelines[static_cast<int>(_id)].Get(); }

	// 指定したIDでRootSinatureを引くことができるGetter
	ID3D12RootSignature* GetRootSignature(RootSigID _id) const { return rootSigs[static_cast<int>(_id)].Get(); }
private:

	// 内蔵ShaderIDからコンパイル済みバイトコードを取得する初回だけコンパイルして2回目以降はキャッシュを返す
	D3D12_SHADER_BYTECODE GetBuiltinShaderBytecode(BuiltinShaderID _id);

private:
	ID3D12Device* device{ nullptr }; // 内部保存するデバイス所有しないので生ポでいい
	ComPtr<ID3D12PipelineState> pipelines[static_cast<int>(PipelineID::Count)]; // パイプラインステート用台帳配列
	ComPtr<ID3D12RootSignature> rootSigs[static_cast<int>(RootSigID::Count)]; // ルートシグネチャ用台帳配列
};
