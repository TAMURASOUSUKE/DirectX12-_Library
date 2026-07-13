#include <algorithm>
#include <string>
#include <d3dcompiler.h>
#include "ShaderSystem.h"

// GraphicsTypeに設定されているenumを実の値へと変換する
namespace
{
	// テクスチャの入力レイアウト
	constexpr  D3D12_INPUT_ELEMENT_DESC TEX_LAYOUT[]
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
	// 3Dモデルの入力レイアウト
	constexpr   D3D12_INPUT_ELEMENT_DESC MODEL_LAYOUT[]
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

	// 2D基本図形の入力レイアウト
	constexpr   D3D12_INPUT_ELEMENT_DESC SHAPE_LAYOUT[]
	{
		// position
		{
			"POSITION", // セマンティクス名
			0,  // セマンティクス番号
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			0, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// color
		{
			"COLOR",  // セマンティクス名
			0,  // セマンティクス番号
			DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT,   // 前の要素の直後に配置
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	// Layoutをenumと同期させる
	struct LayoutEntry
	{
		const D3D12_INPUT_ELEMENT_DESC* elements;
		UINT count;
	};
	// enumと同じ順に並べたlayout
	constexpr LayoutEntry LAYOUT_TABLE[]{
		// None
		{nullptr, 0},
		// Texture
		{ TEX_LAYOUT, _countof(TEX_LAYOUT) },
		// 3DModel
		{MODEL_LAYOUT, _countof(MODEL_LAYOUT)},
		// Shape
		{SHAPE_LAYOUT, _countof(SHAPE_LAYOUT)}
	};
	static_assert(_countof(LAYOUT_TABLE) == static_cast<size_t>(InputLayout::Count), "InputLayoutのID数と実値の総数が合いません\n");

	// 不透明
	constexpr D3D12_RENDER_TARGET_BLEND_DESC BLEND_OPAQUE
	{
		false, // Enable
		false, // LogicOpEnable
		D3D12_BLEND_ONE, // SrcBlend
		D3D12_BLEND_ZERO, // DestBlend
		D3D12_BLEND_OP_ADD, // BlendOp
		D3D12_BLEND_ONE, // SrcBlendAlpha
		D3D12_BLEND_ZERO, // DestBlendAlpha
		D3D12_BLEND_OP_ADD, // BlendOpAlpha
		D3D12_LOGIC_OP_NOOP, // LogicOp
		static_cast<UINT8>(D3D12_COLOR_WRITE_ENABLE_ALL)
	};

	// 透明
	constexpr D3D12_RENDER_TARGET_BLEND_DESC BLEND_ALPHA
	{
		TRUE, // BlendEnable
		FALSE, // LogicOpEnable
		D3D12_BLEND_SRC_ALPHA, // SrcBlend
		D3D12_BLEND_INV_SRC_ALPHA,  // DestBlend
		D3D12_BLEND_OP_ADD, // BlendOp
		D3D12_BLEND_ONE,  // SrcBlendAlpha
		D3D12_BLEND_INV_SRC_ALPHA, // DestBlendAlpha
		D3D12_BLEND_OP_ADD, // BlendOpAlpha
		D3D12_LOGIC_OP_NOOP, // LogicOp
		static_cast<UINT8>(D3D12_COLOR_WRITE_ENABLE_ALL)
	};

	// BlendModeをenumと同期させる
	constexpr D3D12_RENDER_TARGET_BLEND_DESC BLEND_TABLE[]{ BLEND_OPAQUE, BLEND_ALPHA };
	static_assert(_countof(BLEND_TABLE) == static_cast<size_t>(BlendMode::Count), "BlendModeのID数と実値の総数が合いません\n");

	// ステンシルを使わない場合の共通設定
	constexpr D3D12_DEPTH_STENCILOP_DESC STENCIL_DISABLED_OP
	{
		D3D12_STENCIL_OP_KEEP, //StencilFaileOp = stencilテストに失敗したとき
		D3D12_STENCIL_OP_KEEP, //StencilDepthFailOp = stencilテストに成功したが深度テストに失敗したとき
		D3D12_STENCIL_OP_KEEP, //StencilPassOp = stencilテスト、深度テストに成功したとき
		D3D12_COMPARISON_FUNC_ALWAYS, //StencilFunc = stencil比較関数

	};

	// 深度なし
	constexpr D3D12_DEPTH_STENCIL_DESC DEPTH_NONE
	{
		false, // 有効かどうか
		D3D12_DEPTH_WRITE_MASK_ZERO, // 奥行情報を深度バッファに保存しない
		D3D12_COMPARISON_FUNC_ALWAYS,  // DepthFunc(深度比較関数) : 常に比較成功
		false, // Stancil比較しない
		static_cast<UINT8>(D3D12_DEFAULT_STENCIL_READ_MASK), // 全てのビット対象
		static_cast<UINT8>(D3D12_DEFAULT_STENCIL_WRITE_MASK), // 全てのビット対象
		// 表側と裏側の操作
		STENCIL_DISABLED_OP, // FrontFace
		STENCIL_DISABLED_OP  // BackFace
	};

	// 深度書き込み読み込み
	constexpr D3D12_DEPTH_STENCIL_DESC DEPTH_READ_WRITE
	{
		true, // 有効
		D3D12_DEPTH_WRITE_MASK_ALL, // 奥行情報を深度バッファに保存する
		D3D12_COMPARISON_FUNC_LESS, // DepthFunc(深度比較関数) : 手前(数値が小さい)にあれば成功
		false, // stencil比較しない
		static_cast<UINT8>(D3D12_DEFAULT_STENCIL_READ_MASK), // 全てのビット対象
		static_cast<UINT8>(D3D12_DEFAULT_STENCIL_WRITE_MASK), // 全てのビット対象
		// 表側と裏側の操作
		STENCIL_DISABLED_OP, // FrontFace
		STENCIL_DISABLED_OP  // BackFace
	};

	// 深度読み込みのみ
	constexpr D3D12_DEPTH_STENCIL_DESC DEPTH_READ_ONLY
	{
		true, // 有効
		D3D12_DEPTH_WRITE_MASK_ZERO, // 奥行情報を深度バッファに保存しない
		D3D12_COMPARISON_FUNC_LESS, // DepthFunc(深度比較関数) : 手前(数値が小さい)にあれば成功
		false, // stencil比較しない
		static_cast<UINT8>(D3D12_DEFAULT_STENCIL_READ_MASK), // 全てのビット対象
		static_cast<UINT8>(D3D12_DEFAULT_STENCIL_WRITE_MASK), // 全てのビット対象
		// 表側と裏側の操作
		STENCIL_DISABLED_OP, // FrontFace
		STENCIL_DISABLED_OP  // BackFace
	};

	// ステートとフォーマットをまとめる
	struct DepthEntry
	{
		D3D12_DEPTH_STENCIL_DESC state{};
		DXGI_FORMAT dsvFormat;
	};

	constexpr DepthEntry DEPTH_TABLE[]
	{
		// None
		{DEPTH_NONE, DXGI_FORMAT_UNKNOWN},
		// ReadWrite
		{DEPTH_READ_WRITE, DXGI_FORMAT_D24_UNORM_S8_UINT},
		// ReadOnly
		{DEPTH_READ_ONLY, DXGI_FORMAT_D24_UNORM_S8_UINT}
	};
	static_assert(_countof(DEPTH_TABLE) == static_cast<size_t>(DepthParam::Count),"DepthParamのID数と実値の総数が合いません\n");

	// 共通部品作成ヘルパー関数
	RootParamDesc MakeRootCBV(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility)
	{
		RootParamDesc desc{};
		desc.type = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファに設定
		desc.shaderRegister = _shaderRegister;
		desc.registerSpace = 0;
		desc.visibility = _visibility;
		return desc;
	}

	RootParamDesc MakeSRVTable(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility)
	{
		RootParamDesc desc{};
		desc.type = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタ―テーブル指定
		desc.visibility = _visibility;

		DescriptorRangeDesc range{}; // レンジ設定
		range.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV指定
		range.numDescriptors = 1;
		range.baseShaderRegister = _shaderRegister;
		range.registerSpace = 0;
		desc.ranges.push_back(range);
		return desc;
	}

	// 線形での繰り返しを取るサンプラー設定
	D3D12_STATIC_SAMPLER_DESC MakeLinearWrapSampler(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility)
	{
		D3D12_STATIC_SAMPLER_DESC desc{};
		desc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // 画像の拡縮補間を線形で

		desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 繰り返し
		desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 繰り返し
		desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // 繰り返し

		desc.MipLODBias = 0.0f; // 遠景用画像に切り替わる度合(基準)
		desc.MaxAnisotropy = 1; // 異方性フィルタリングの倍率
		desc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;  // 色の比較テストを行わない
		desc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK; // 画像の範囲外を黒で塗りつぶす
		desc.MinLOD = 0.0f; // ミップレベルの使用下限は0
		desc.MaxLOD = D3D12_FLOAT32_MAX; // ミップレベルの使用上限は最大
		desc.ShaderRegister = _shaderRegister;
		desc.RegisterSpace = 0;
		desc.ShaderVisibility = _visibility;
		return desc;
	}

}

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

bool ShaderSystem::CreateRootSignature(const RootSignatureDesc& _desc)
{
	return true;
}

bool ShaderSystem::CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc)
{
	return true;
}

bool ShaderSystem::CreateComputePipeline(const ComputePipelineDesc& _desc)
{
	return true;
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

ComPtr<ID3D12RootSignature> ShaderSystem::CreateShapeRootSignature()
{
	HRESULT result{}; // 結果判定用オブジェクト

	D3D12_ROOT_PARAMETER rootParamCBV{};
	rootParamCBV.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // TypeはCSVに指定
	rootParamCBV.ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 定数バッファはVSに置いてあるのでVERTEX指定
	rootParamCBV.Descriptor.RegisterSpace = 0; // レジスタオフセット
	rootParamCBV.Descriptor.ShaderRegister = 0; // b0

	// ルートシグネチャの設定構造体
	D3D12_ROOT_SIGNATURE_DESC rootSigDesc{};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSigDesc.NumParameters = 1;
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
ComPtr<ID3D12PipelineState> ShaderSystem::CreateShapePipeLineState(ID3D12RootSignature* _rootSig, ID3DBlob* _vsBlob, ID3DBlob* _psBlob, bool _isWireFlag)
{
	HRESULT result{}; // 結果判定用構造体
	
	D3D12_INPUT_ELEMENT_DESC inputLayout[]
	{
		// position
		{
			"POSITION", // セマンティクス名
			0,  // セマンティクス番号
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			0, // 頂点構造体のオフセット
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// color
		{
			"COLOR",  // セマンティクス名
			0,  // セマンティクス番号
			DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
			0, // 入力スロット
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

	// プリミティブ形状(フラグによってwireかどうかを分ける)
	pipelineDesc.PrimitiveTopologyType = (_isWireFlag) ? D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE : D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

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

