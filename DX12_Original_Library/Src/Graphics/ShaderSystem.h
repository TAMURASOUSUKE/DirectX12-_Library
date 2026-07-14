#pragma once
#include <d3d12.h>
#include <wrl/client.h>
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

	void Initialize(ID3D12Device* _device); // 初期化処理
	void Shutdown(); // 終了処理

	ComPtr<ID3DBlob> Compile(const wchar_t* _filePath, const char* _entryPoint, const char* _target); // HLSLシェーダーをコンパイルする(ファイル名は日本語が含まれる可能性を考慮しワイド文字)
	
	// ShapeFillとWireは共通のRootSignatureのため共通関数を残す
	bool CreateRootSignature(const RootSignatureDesc& _desc);
	// VS,HS,DS,GS,PS用のGraphicsPipelineを生成する
	bool CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc);
	// CS用のPipelineを生成する
	bool CreateComputePipeline(const ComputePipelineDesc& _desc);

	// 共通部品作成ヘルパー関数
	RootParamDesc MakeRootCBV(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility); // CBV作成
	RootParamDesc MakeSRVTable(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility); // DescriptorTableでのSRV作成
	D3D12_STATIC_SAMPLER_DESC MakeLinearWrapSampler(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility); // 線形での繰り返しを取るサンプラー設定

	ComPtr<ID3D12RootSignature> CreateTextureRootSignature(); // テクスチャ表示用ルートシグネチャの作成
	ComPtr<ID3D12PipelineState> CreateTexturePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob); // テクスチャ表示用パイプラインステートオブジェクトの作成

	ComPtr<ID3D12RootSignature> CreateModelRootSignature(); // モデル表示用ルートシグネチャの作成
	ComPtr<ID3D12PipelineState> CreateModelPipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob); // モデル表示用パイプラインステート作成

	ComPtr<ID3D12RootSignature> CreateShapeRootSignature(); // 2D基礎図形描画用
	ComPtr<ID3D12PipelineState> CreateShapePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBolb, ID3DBlob* _psBlob, bool _isWireFlag );

	// 指定したIDでPipelineを引くことができるGetter
	ID3D12PipelineState* GetPipeline(PipelineID _id) const { return pipelines[static_cast<int>(_id)].Get(); }
	// 指定したIDでRootSinatureを引くことができるGetter
	ID3D12RootSignature* GetRootSignature(RootSigID _id) const { return rootSigs[static_cast<int>(_id)].Get(); }
private:
	ID3D12Device* device{ nullptr }; // 内部保存するデバイス所有しないので生ポでいい
	ComPtr<ID3D12PipelineState> pipelines[static_cast<int>(PipelineID::Count)]; // パイプラインステート用台帳配列
	ComPtr<ID3D12RootSignature> rootSigs[static_cast<int>(RootSigID::Count)]; // ルートシグネチャ用台帳配列

};