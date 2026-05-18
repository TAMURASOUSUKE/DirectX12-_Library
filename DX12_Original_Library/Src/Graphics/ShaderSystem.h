#pragma once
#include <windows.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
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

	ComPtr<ID3DBlob> Compile(const wchar_t* filePath, const char* entryPoint, const char* _target); // HLSLシェーダーをコンパイルする(ファイル名は日本語額含まれる可能性を考慮しワイド文字)
	ComPtr<ID3D12RootSignature> CreateRootSignature(); // ルートシグネチャの作成
	ComPtr<ID3D12PipelineState> CreatePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBolb, ID3DBlob _psBolb); // パイプラインステートオブジェクトの作成

private:
	ComPtr<ID3D12Device> device; // 内部保存するデバイス
};