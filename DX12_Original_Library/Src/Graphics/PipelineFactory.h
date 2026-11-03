#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "GraphicsType.h"
using Microsoft::WRL::ComPtr;

// ShaderのコンパイルとRootSignatureとPSOの生成を行う関数群
namespace PipelineFactory
{
	// HLSLシェーダーをコンパイルする(ファイル名は日本語が含まれる可能性を考慮しワイド文字)
	ComPtr<ID3DBlob> Compile(const wchar_t* _filePath, const char* _entryPoint, const char* _target);

	// コンパイル済みShaderとPSO設定からGraphicsPipelineを組み立てる
	ComPtr<ID3D12PipelineState> BuildGraphicsPipeline(ID3D12Device* _device, ID3D12RootSignature* _rootSig, const GraphicsPipelineDesc& _desc,
		const D3D12_SHADER_BYTECODE& _vs, const D3D12_SHADER_BYTECODE& _ps,
		const D3D12_SHADER_BYTECODE& _hs = {}, const D3D12_SHADER_BYTECODE& _ds = {},
		const D3D12_SHADER_BYTECODE& _gs = {});

	// コンパイル済みCSとRootSigからPSOを作成する
	ComPtr<ID3D12PipelineState> BuildComputePipeline(ID3D12Device* _device, ID3DBlob* _cs, ID3D12RootSignature* _rootSignature);

	// Graphics用の共通RootSignatureを作る
	ComPtr<ID3D12RootSignature> BuildGraphicsRootSignature(ID3D12Device* _device, const RootSignatureDesc& _desc);

	// Compute用の共通RootSignatureを作成する
	ComPtr<ID3D12RootSignature> BuildComputeRootSignature(ID3D12Device* _device);
}
