#pragma once
#include <d3d12.h>
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

	ComPtr<ID3DBlob> Compile(const wchar_t* _filePath, const char* _entryPoint, const char* _target); // HLSLシェーダーをコンパイルする(ファイル名は日本語額含まれる可能性を考慮しワイド文字)
	
	ComPtr<ID3D12RootSignature> CreateTextureRootSignature(); // テクスチャ表示用ルートシグネチャの作成
	ComPtr<ID3D12PipelineState> CreateTexturePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob); // テクスチャ表示用パイプラインステートオブジェクトの作成

	ComPtr<ID3D12RootSignature> CreateModelRootSignature(); // モデル表示用ルートシグネチャの作成
	ComPtr<ID3D12PipelineState> CreateModelPipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob); // モデル表示用パイプラインステート作成

	// 以下デバッグ用の関数(仮で動かすためのPSOやルートシグネチャ設定)
	ComPtr<ID3D12RootSignature> CreateDebugTriangleRootSignature(); // 三角形表示用ルートシグネチャの作成
	ComPtr<ID3D12PipelineState> CreateDebugTriaglePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob); // 三角形表示用パイプラインステートオブジェクトの作成
	ComPtr<ID3D12RootSignature> CreateDebugCubeRootSignature(); // キューブ表示用ルートシグネチャの作成
	ComPtr<ID3D12PipelineState> CreateDebugCubePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob); // キューブ表示用パイプラインステートオブジェクトの作成
private:
	ID3D12Device* device; // 内部保存するデバイス所有しないので生ポでいい
};