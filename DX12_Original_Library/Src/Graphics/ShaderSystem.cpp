#include <algorithm>
#include <string>
#include <d3dcompiler.h>
#include "ShaderSystem.h"

// 初期化処理
void ShaderSystem::Initialize(ID3D12Device* _device)
{
	// 作成されたデバイスと結合
	if (_device != nullptr)
	{
		device = _device;
	}
}


// 終了処理
void ShaderSystem::Shutdown()
{

}

// Shaderのコンパイル
ComPtr<ID3DBlob> ShaderSystem::Compile(const wchar_t* _filePath, const char* _entryPoint, const char* _target)
{
	HRESULT result{}; // 作成結果を格納するオブジェクト
	ComPtr<ID3DBlob> compiledShader{ nullptr }; // コンパイルされたシェーダーが格納される
	ComPtr<ID3DBlob> errorBlob{ nullptr }; // エラーが起こった時の対処用

	UINT compileFlags{ 0 }; // コンパイルする際のオプション

	// デバッグ時はデバッグ用のオプションにする
#ifdef _DEBUG
	compileFlags |= D3DCOMPILE_DEBUG; // デバッグ用のフラグ
	compileFlags |= D3DCOMPILE_SKIP_OPTIMIZATION; // シェーダーの最適化を行わない
#endif // _DEBUG

	// コンパイル処理
	result = D3DCompileFromFile(
		_filePath, // ファイルパス
		nullptr, // マクロオブジェクト
		D3D_COMPILE_STANDARD_FILE_INCLUDE, // インクルードオブジェクト(includeを使えるようにする)
		_entryPoint, // エントリーポイント
		_target, // ターゲット(vs,psなど)
		compileFlags, // コンパイルオプション
		0, // エフェクトコンパイルオプション
		&compiledShader, // 格納するためのポインタのアドレス
		&errorBlob // エラー用のポインタのアドレス
	);

	// 失敗したときの処理
	if (FAILED(result))
	{
		// ファイル名が見当たらない時の処理
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND))
		{
			OutputDebugStringA("ファイルが見つかりません\n");
			OutputDebugStringW(_filePath); // ワイド文字でファイル名を出力しどのファイルが見つからなかったかをわかりやすくする
			OutputDebugStringW(L"\n"); // 改行
			return nullptr;
		}

		// エラー時はerrorBolbにメッセージが入るため取り出す
		if (errorBlob)
		{
			const char* errorMessage{ static_cast<const char*>(errorBlob->GetBufferPointer()) }; // バッファの先頭アドレスを取得し文字列に変換する
			std::string errorStr{}; // エラー文字列格納用
			errorStr.assign(errorMessage, errorBlob->GetBufferSize()); // 先頭から文字列のサイズ分だけ再代入する
			errorStr += "\n"; // 改行
			OutputDebugStringA(errorStr.c_str()); // char型を取り出し出力
		}
		else
		{
			// コンパイルに失敗したメッセージを出力
			OutputDebugStringA("Shaderをコンパイルできませんでした\n");
		}
		return nullptr; // 失敗ならnullを返す
	}

	return compiledShader; // コンパイルされたShaderのオブジェクトを返す
}

// ルートシグネチャの作成
ComPtr<ID3D12RootSignature> ShaderSystem::CreateTextureRootSignature()
{
	HRESULT result{}; // 結果確認用オブジェクト

	// ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // テクスチャなのでSRV
	descriptorRange.NumDescriptors = 1; // テクスチャ一つなので1
	descriptorRange.BaseShaderRegister = 0; // 0番スロットから始める
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 前のレンジの直後に配置する

	// ルートパラメータの設定
	D3D12_ROOT_PARAMETER rootParam{};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルタイプに指定する
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーから見えるようにする
	rootParam.DescriptorTable.pDescriptorRanges = &descriptorRange; // ディスクリプタレンジのアドレス
	rootParam.DescriptorTable.NumDescriptorRanges = 1; // ディスクリプタレンジの数

	D3D12_ROOT_PARAMETER rootParamCBV{};
	rootParamCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // TypeはCSVに指定
	rootParamCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 定数バッファはVSに置いてあるのでVERTEX指定
	rootParamCBV.Descriptor.RegisterSpace = 0; // レジスタオフセット
	rootParamCBV.Descriptor.ShaderRegister = 0; // b0

	D3D12_ROOT_PARAMETER rootPrams[]{ rootParam, rootParamCBV }; // パラメータの配列

	// サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC smpDesc{};
	smpDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 線形補完
	// 繰り返し
	smpDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.ShaderRegister = 0; // s0設定
	smpDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// ルートシグネチャの設定構造体
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.NumParameters = 2;
	rootSigDesc.NumStaticSamplers = 1;
	rootSigDesc.pParameters = rootPrams; // 配列を渡す
	rootSigDesc.pStaticSamplers = &smpDesc;

	// バイナリコードの作成
	ComPtr<ID3DBlob> rootSigBlob{ nullptr };
	ComPtr<ID3DBlob> errorBlob{ nullptr }; // エラーが起こった時の対処用
	result = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	if (FAILED(result))
	{
		if (errorBlob)
		{
			OutputDebugStringA(
				static_cast<const char*>(errorBlob->GetBufferPointer())
			);
		}
		return nullptr;
	}

	// ルートシグネチャの作成
	ComPtr<ID3D12RootSignature> rootSig{};
	result = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	if (FAILED(result))
	{
		return nullptr;
	}

	return rootSig;
}

// ルートシグネチャの作成
ComPtr<ID3D12RootSignature> ShaderSystem::CreateDebugTriangleRootSignature()
{
	HRESULT result{}; // 結果判定用オブジェクト

	// ルートシグネチャの設定構造体
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// バイナリコードの作成
	ComPtr<ID3DBlob> rootSigBlob{ nullptr };
	ComPtr<ID3DBlob> errorBlob{ nullptr }; // エラーが起こった時の対処用
	result = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	if (FAILED(result))
	{
		if (errorBlob)
		{
			OutputDebugStringA(
				static_cast<const char*>(errorBlob->GetBufferPointer())
			);
		}
		return nullptr;
	}

	// ルートシグネチャの作成
	ComPtr<ID3D12RootSignature> rootSig{};
	result = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	if (FAILED(result))
	{
		return nullptr;
	}

	return rootSig;
}

// パイプラインステートの作成
ComPtr<ID3D12PipelineState> ShaderSystem::CreateTexturePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob)
{
	HRESULT result{}; // 結果格納

	D3D12_INPUT_ELEMENT_DESC inputLayout[]
	{
		// positionのセマンティクス
		{
			"POSITION", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			0, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// UV
		{
			"TEXCOORD", // HLSL側のセマンティクス
			0,
			DXGI_FORMAT_R32G32_FLOAT, // float2
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,   // 前の要素の直後に配置
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{}; // パイプラインステート設定構造体
	pipelineDesc.pRootSignature = _rootSig; // ルートシグネチャ
	// VSShader
	pipelineDesc.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = _vsBlob->GetBufferSize();

	// PSShader
	pipelineDesc.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = _psBlob->GetBufferSize();

	// 入力レイアウト
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

	// プリミティブ形状
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形

	// ラスタライザ設定
	pipelineDesc.RasterizerState.MultisampleEnable = false; // アンチエイリアスは使わない
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
	pipelineDesc.RasterizerState.DepthClipEnable = true; // 深度方向のクリッピングを有効化

	// サンプルマスク
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // (0xffffffff)

	// ブレンドステート設定構造体
	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc{};
	renderTargetBlendDesc.BlendEnable = true; // ブレンドを行うかどうか
	renderTargetBlendDesc.LogicOpEnable = false; // 論理演算するかどうか
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // 全ての要素をブレンドする
	renderTargetBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA; // ソース元のアルファ値を係数として扱う
	renderTargetBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA; // 残りのアルファ値を既存の色に対してかける
	renderTargetBlendDesc.BlendOp = D3D12_BLEND_OP_ADD; // 上記二つを加算させる
	renderTargetBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE; // アルファ値そのまま
	renderTargetBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO; // 0
	renderTargetBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD; // 加算

	// ブレンドステート設定
	pipelineDesc.BlendState.AlphaToCoverageEnable = false; // αテストなし
	pipelineDesc.BlendState.IndependentBlendEnable = false; // それぞれのパイプラインステートに対して個別のブレンドステートを割り当てるか
	pipelineDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// レンダーターゲット設定
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// 深度テスト設定を無効
	pipelineDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	// マルチサンプリング設定
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleDesc.Quality = 0;

	ComPtr<ID3D12PipelineState> pipelineState{}; // パイプラインステートオブジェクト
	result = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(result)) return nullptr;
	return pipelineState;
}

ComPtr<ID3D12RootSignature> ShaderSystem::CreateModelRootSignature()
{
	HRESULT result{}; // 結果確認用オブジェクト

	// ディスクリプタレンジの設定
	D3D12_DESCRIPTOR_RANGE descriptorRange{};
	descriptorRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // テクスチャなのでSRV
	descriptorRange.NumDescriptors = 1; // テクスチャ一つなので1
	descriptorRange.BaseShaderRegister = 0; // 0番スロットから始める
	descriptorRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // 前のレンジの直後に配置する

	D3D12_ROOT_PARAMETER rootPrams[4]{ }; // パラメータの配列
	// ルートパラメータの設定
	// MVP用
	rootPrams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // TypeはCSVに指定
	rootPrams[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 定数バッファはVSに置いてあるのでVERTEX指定
	rootPrams[0].Descriptor.RegisterSpace = 0; // レジスタオフセット
	rootPrams[0].Descriptor.ShaderRegister = 0; // b0

	// material用
	rootPrams[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // TypeはCBVに指定
	rootPrams[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // materialはピクセルシェーダーで読むのでへ
	rootPrams[1].Descriptor.RegisterSpace = 0; // オフセット0
	rootPrams[1].Descriptor.ShaderRegister = 1; // b1

	// スキニング行列用
	rootPrams[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // TypeはCBVに指定
	rootPrams[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // スキニング行列は頂点シェーダーで読むのでへ
	rootPrams[2].Descriptor.RegisterSpace = 0; // オフセット0
	rootPrams[2].Descriptor.ShaderRegister = 2; // b2

	// テクスチャ(t0)
	rootPrams[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルタイプに指定する
	rootPrams[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーから見えるようにする
	rootPrams[3].DescriptorTable.pDescriptorRanges = &descriptorRange; // ディスクリプタレンジのアドレス
	rootPrams[3].DescriptorTable.NumDescriptorRanges = 1; // ディスクリプタレンジの数

	// サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC smpDesc{};
	smpDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 線形補完
	// 繰り返し
	smpDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	smpDesc.ShaderRegister = 0; // s0設定
	smpDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	// ルートシグネチャの設定構造体
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.NumParameters = 4;
	rootSigDesc.NumStaticSamplers = 1;
	rootSigDesc.pParameters = rootPrams; // 配列を渡す
	rootSigDesc.pStaticSamplers = &smpDesc;

	// バイナリコードの作成
	ComPtr<ID3DBlob> rootSigBlob{ nullptr };
	ComPtr<ID3DBlob> errorBlob{ nullptr }; // エラーが起こった時の対処用
	result = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	if (FAILED(result))
	{
		if (errorBlob)
		{
			OutputDebugStringA(
				static_cast<const char*>(errorBlob->GetBufferPointer())
			);
		}
		return nullptr;
	}

	// ルートシグネチャの作成
	ComPtr<ID3D12RootSignature> rootSig{};
	result = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	if (FAILED(result))
	{
		return nullptr;
	}

	return rootSig;
}

ComPtr<ID3D12PipelineState> ShaderSystem::CreateModelPipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob)
{
	HRESULT result{}; // 結果格納

	D3D12_INPUT_ELEMENT_DESC inputLayout[]
	{
		// positionのセマンティクス
		{
			"POSITION", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// 法線
		{
			"NORMAL", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// UV
		{
			"TEXCOORD", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32_FLOAT, // float2
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		// 重み
		{
			"WEIGHTS", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		// ボーン
		{
			"BONES", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32B32A32_UINT, // uint32 * 4
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT, // 頂点構造体オフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{}; // パイプラインステート設定構造体
	pipelineDesc.pRootSignature = _rootSig; // ルートシグネチャ
	// VSShader
	pipelineDesc.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = _vsBlob->GetBufferSize();

	// PSShader
	pipelineDesc.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = _psBlob->GetBufferSize();

	// 入力レイアウト
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

	// プリミティブ形状
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形

	// ラスタライザ設定
	pipelineDesc.RasterizerState.MultisampleEnable = false; // アンチエイリアスは使わない
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
	pipelineDesc.RasterizerState.DepthClipEnable = true; // 深度方向のクリッピングを有効化

	// サンプルマスク
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // (0xffffffff)

	// ブレンドステート設定構造体
	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc{};
	renderTargetBlendDesc.BlendEnable = false; // ブレンドを行うかどうか
	renderTargetBlendDesc.LogicOpEnable = false; // 論理演算するかどうか
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // 全ての要素をブレンドする

	// ブレンドステート設定
	pipelineDesc.BlendState.AlphaToCoverageEnable = false; // αテストなし
	pipelineDesc.BlendState.IndependentBlendEnable = false; // それぞれのパイプラインステートに対して個別のブレンドステートを割り当てるか
	pipelineDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// レンダーターゲット設定
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// 深度テスト設定を有効化
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.DepthStencilState.DepthEnable = true;
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 深度地をバッファに書き込む
	pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 既存の値より手前なら通す

	// マルチサンプリング設定
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleDesc.Quality = 0;

	ComPtr<ID3D12PipelineState> pipelineState{}; // パイプラインステートオブジェクト
	result = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(result)) return nullptr;
	return pipelineState;
}

ComPtr<ID3D12PipelineState> ShaderSystem::CreateDebugCubePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob)
{
	HRESULT result{}; // 結果格納

	D3D12_INPUT_ELEMENT_DESC inputLayout[]
	{
		// positionのセマンティクス
		{
			"POSITION", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			0, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// Color
		{
			"COLOR", // HLSL側のセマンティクス
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,   // 前の要素の直後に配置
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{}; // パイプラインステート設定構造体
	pipelineDesc.pRootSignature = _rootSig; // ルートシグネチャ
	// VSShader
	pipelineDesc.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = _vsBlob->GetBufferSize();

	// PSShader
	pipelineDesc.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = _psBlob->GetBufferSize();

	// 入力レイアウト
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

	// プリミティブ形状
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形

	// ラスタライザ設定
	pipelineDesc.RasterizerState.MultisampleEnable = false; // アンチエイリアスは使わない
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
	pipelineDesc.RasterizerState.DepthClipEnable = true; // 深度方向のクリッピングを有効化

	// サンプルマスク
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // (0xffffffff)

	// ブレンドステート設定構造体
	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc{};
	renderTargetBlendDesc.BlendEnable = false; // ブレンドを行うかどうか
	renderTargetBlendDesc.LogicOpEnable = false; // 論理演算するかどうか
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // 全ての要素をブレンドする

	// ブレンドステート設定
	pipelineDesc.BlendState.AlphaToCoverageEnable = false; // αテストなし
	pipelineDesc.BlendState.IndependentBlendEnable = false; // それぞれのパイプラインステートに対して個別のブレンドステートを割り当てるか
	pipelineDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// レンダーターゲット設定
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// 深度テスト設定を有効化
	pipelineDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineDesc.DepthStencilState.DepthEnable = true;
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL; // 深度地をバッファに書き込む
	pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS; // 既存の値より手前なら通す

	// マルチサンプリング設定
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleDesc.Quality = 0;

	ComPtr<ID3D12PipelineState> pipelineState{}; // パイプラインステートオブジェクト
	result = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(result)) return nullptr;
	return pipelineState;
}

// キューブ用ルートシグネチャの作成
ComPtr<ID3D12RootSignature> ShaderSystem::CreateDebugCubeRootSignature()
{
	HRESULT result{}; // 結果確認用オブジェクト

	D3D12_ROOT_PARAMETER rootParamCBV{};
	rootParamCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // TypeはCSVに指定
	rootParamCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 定数バッファはVSに置いてあるのでVERTEX指定
	rootParamCBV.Descriptor.RegisterSpace = 0; // レジスタオフセット
	rootParamCBV.Descriptor.ShaderRegister = 0; // b0

	// ルートシグネチャの設定構造体
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.NumParameters = 1;
	rootSigDesc.NumStaticSamplers = 0;
	rootSigDesc.pParameters = &rootParamCBV;

	// バイナリコードの作成
	ComPtr<ID3DBlob> rootSigBlob{ nullptr };
	ComPtr<ID3DBlob> errorBlob{ nullptr }; // エラーが起こった時の対処用
	result = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &rootSigBlob, &errorBlob);
	if (FAILED(result))
	{
		if (errorBlob)
		{
			OutputDebugStringA(
				static_cast<const char*>(errorBlob->GetBufferPointer())
			);
		}
		return nullptr;
	}

	// ルートシグネチャの作成
	ComPtr<ID3D12RootSignature> rootSig{};
	result = device->CreateRootSignature(0, rootSigBlob->GetBufferPointer(), rootSigBlob->GetBufferSize(), IID_PPV_ARGS(&rootSig));
	if (FAILED(result))
	{
		return nullptr;
	}

	return rootSig;
}

// パイプラインステートの作成
ComPtr<ID3D12PipelineState> ShaderSystem::CreateDebugTriaglePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob)
{
	HRESULT result{}; // 結果格納

	D3D12_INPUT_ELEMENT_DESC inputLayout[]
	{
		// positionのセマンティクス
		{
			"POSITION", // HLSL側のセマンティクス
			0, // セマンティクス番号
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			0, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// 頂点カラー
		{
			"COLOR", // HLSL側のセマンティクス
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,   // 前の要素の直後に配置
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc{}; // パイプラインステート設定構造体
	pipelineDesc.pRootSignature = _rootSig; // ルートシグネチャ
	// VSShader
	pipelineDesc.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	pipelineDesc.VS.BytecodeLength = _vsBlob->GetBufferSize();

	// PSShader
	pipelineDesc.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	pipelineDesc.PS.BytecodeLength = _psBlob->GetBufferSize();

	// 入力レイアウト
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof(inputLayout);

	// プリミティブ形状
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE; // 三角形

	// ラスタライザ設定
	pipelineDesc.RasterizerState.MultisampleEnable = false; // アンチエイリアスは使わない
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // カリングしない
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID; // 中身を塗りつぶす
	pipelineDesc.RasterizerState.DepthClipEnable = true; // 深度方向のクリッピングを有効化

	// サンプルマスク
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK; // (0xffffffff)

	// ブレンドステート設定構造体
	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc{};
	renderTargetBlendDesc.BlendEnable = false; // ブレンドを行うかどうか
	renderTargetBlendDesc.LogicOpEnable = false; // 論理演算するかどうか
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL; // 全ての要素をブレンドする

	// ブレンドステート設定
	pipelineDesc.BlendState.AlphaToCoverageEnable = false; // αテストなし
	pipelineDesc.BlendState.IndependentBlendEnable = false; // それぞれのパイプラインステートに対して個別のブレンドステートを割り当てるか
	pipelineDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// レンダーターゲット設定
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// 深度バッファを使わないのでUNKNOWN
	pipelineDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;

	// マルチサンプリング設定
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleDesc.Quality = 0;

	ComPtr<ID3D12PipelineState> pipelineState{}; // パイプラインステートオブジェクト
	result = device->CreateGraphicsPipelineState(&pipelineDesc, IID_PPV_ARGS(&pipelineState));
	if (FAILED(result)) return nullptr;
	return pipelineState;
}

