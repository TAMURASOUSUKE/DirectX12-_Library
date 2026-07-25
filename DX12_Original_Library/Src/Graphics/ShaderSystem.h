#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "d3dcompiler.lib")

// ShaderをコンパイルしたりPSOやルートシグネチャの作成を行う
class ShaderSystem
{
public:
	ShaderSystem() = default; // デフォルトコンストラクタ(ファサードから使われないためシングルトンにする必要はない)
	~ShaderSystem() = default; // デフォルトデストラクタ

	void Setup(ID3D12Device* _device); // 初期化処理
	void Shutdown(); // 終了処理

	ComPtr<ID3DBlob> Compile(const wchar_t* _filePath, const char* _entryPoint, const char* _target); // HLSLシェーダーをコンパイルする(ファイル名は日本語が含まれる可能性を考慮しワイド文字)
	
	// ShapeFillとWireは共通のRootSignatureのため共通関数を残す
	bool CreateRootSignature(const RootSignatureDesc& _desc);
	// VS,HS,DS,GS,PS用のGraphicsPipelineを生成する
	bool CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc);
	// CS用のPipelineを生成する
	bool CreateComputePipeline(const ComputePipelineDesc& _desc);
	// Material用のPSOを作成する作成したPSOは固定Pipeline台帳には登録せず呼び出し元へ返す
	ComPtr<ID3D12PipelineState> CreateMaterialPipeline(ShaderUsage _usage, ID3DBlob* _vertexShader, ID3DBlob* _pixelShader);
	// 汎用RootSignatureDescを作成する関数
	std::vector<RootSignatureDesc> MakeRootSignatureDescs() const;

	// 指定したIDでPipelineを引くことができるGetter
	ID3D12PipelineState* GetPipeline(PipelineID _id) const { return pipelines[static_cast<int>(_id)].Get(); }
	// 指定したIDでRootSinatureを引くことができるGetter
	ID3D12RootSignature* GetRootSignature(RootSigID _id) const { return rootSigs[static_cast<int>(_id)].Get(); }
private:
	// コンパイル済みShaderとPSO設定から、GraphicsPipelineを組み立てる
	ComPtr<ID3D12PipelineState> BuildGraphicsPipeline(const GraphicsPipelineDesc& _desc, const D3D12_SHADER_BYTECODE& _vs, const D3D12_SHADER_BYTECODE& _ps, const D3D12_SHADER_BYTECODE& _hs = {}, const D3D12_SHADER_BYTECODE& _ds = {}, const D3D12_SHADER_BYTECODE& _gs = {});

private:
	ID3D12Device* device{ nullptr }; // 内部保存するデバイス所有しないので生ポでいい
	ComPtr<ID3D12PipelineState> pipelines[static_cast<int>(PipelineID::Count)]; // パイプラインステート用台帳配列
	ComPtr<ID3D12RootSignature> rootSigs[static_cast<int>(RootSigID::Count)]; // ルートシグネチャ用台帳配列
	ComPtr<ID3DBlob> defaultPostEffectVS{}; // 同じHLSLをMaterial作成ごとに再コンパイルしないため保持する
	ComPtr<ID3DBlob> defaultSpriteVS{}; // 上記と同様の目的のSprite版

};
