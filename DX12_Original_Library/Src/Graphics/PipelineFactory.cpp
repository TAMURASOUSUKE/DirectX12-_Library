#include <d3dcompiler.h>
#include <vector>
#include "GraphicsType.h"
#include "../Debug/DebugLogs.h"
#include "../Core/TextEncoding.h"
#include "PipelineFactory.h"
#pragma comment(lib, "d3dcompiler.lib")

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
			D3D12_APPEND_ALIGNED_ELEMENT,   // 前の要素の直後に配置
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

	// Spriteの入力レイアウト
	constexpr D3D12_INPUT_ELEMENT_DESC SPRITE_LAYOUT[]
	{
		// positionのセマンティクス
		{
			"POSITION", // HLSL側のセマンティクス
			0, // セマンティクス番号 
			DXGI_FORMAT_R32G32B32_FLOAT, // float3
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT, // 前の要素の直後に配置
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		// uv
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT, // float2
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},

		// color
		{
			"COLOR",
			0,
			DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
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
			D3D12_APPEND_ALIGNED_ELEMENT,   // 前の要素の直後に配置
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

	// 3D基礎図形の単位メッシュ用入力レイアウト
	constexpr D3D12_INPUT_ELEMENT_DESC PRIMITIVE_3D_LAYOUT[]
	{
		// position
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
		// normal
		{
			"NORMAL",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
	};

	// VFX用の入力レイアウト
	constexpr D3D12_INPUT_ELEMENT_DESC  VFX_MESH_LAYOUT[]
	{
		// position
		{
			"POSITION",
			0,
			DXGI_FORMAT_R32G32B32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
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
		// color
		{
			"COLOR",  // セマンティクス名
			0,  // セマンティクス番号
			DXGI_FORMAT_R32G32B32A32_FLOAT, // float4
			0, // 入力スロット
			D3D12_APPEND_ALIGNED_ELEMENT,   // 前の要素の直後に配置
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		},
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
		// Sprite
		{SPRITE_LAYOUT, _countof(SPRITE_LAYOUT)},
		// 3DModel
		{MODEL_LAYOUT, _countof(MODEL_LAYOUT)},
		// Shape
		{SHAPE_LAYOUT, _countof(SHAPE_LAYOUT)},
		// Primitive3D
		{PRIMITIVE_3D_LAYOUT, _countof(PRIMITIVE_3D_LAYOUT)},
		// VFX
		{VFX_MESH_LAYOUT, _countof(VFX_MESH_LAYOUT) }
	};
	static_assert(_countof(LAYOUT_TABLE) == static_cast<size_t>(InputLayout::Count), "InputLayoutのID数と実値の総数が合いません\n");

	// 不透明
	constexpr D3D12_RENDER_TARGET_BLEND_DESC BLEND_OPAQUE
	{
		FALSE, // Enable
		FALSE, // LogicOpEnable
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

	// 加算
	constexpr D3D12_RENDER_TARGET_BLEND_DESC BLEND_ADDITIVE
	{
		TRUE, // BlendEnable
		FALSE, // LogicOpEnable
		D3D12_BLEND_SRC_ALPHA, // SrcBlend
		D3D12_BLEND_ONE,  // DestBlend
		D3D12_BLEND_OP_ADD, // BlendOp
		D3D12_BLEND_ONE,  // SrcBlendAlpha
		D3D12_BLEND_ONE, // DestBlendAlpha
		D3D12_BLEND_OP_ADD, // BlendOpAlpha
		D3D12_LOGIC_OP_NOOP, // LogicOp
		static_cast<UINT8>(D3D12_COLOR_WRITE_ENABLE_ALL)
	};

	// BlendModeをenumと同期させる
	constexpr D3D12_RENDER_TARGET_BLEND_DESC BLEND_TABLE[]{ BLEND_OPAQUE, BLEND_ALPHA, BLEND_ADDITIVE };
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
		{DEPTH_READ_ONLY, DXGI_FORMAT_D24_UNORM_S8_UINT},
		// ShadowWrite
		{DEPTH_READ_WRITE, DXGI_FORMAT_D32_FLOAT},
	};
	static_assert(_countof(DEPTH_TABLE) == static_cast<size_t>(DepthParam::Count), "DepthParamのID数と実値の総数が合いません\n");

	struct ColorTargetEntry
	{
		UINT count;
		DXGI_FORMAT format;
	};

	constexpr ColorTargetEntry COLOR_TARGET_TABLE[]
	{
		// SceneColor
		{ 1, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB },
		// None
		{ 0, DXGI_FORMAT_UNKNOWN }, // ShadowPassは色を描画せず、DSVだけを使用する
	};

	static_assert(_countof(COLOR_TARGET_TABLE) == static_cast<size_t>(ColorTargetParam::Count), "ColorTargetParamのID数と実値の総数が合いません\n");
}

ComPtr<ID3DBlob> PipelineFactory::Compile(const wchar_t* _filePath, const char* _entryPoint, const char* _target)
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
			if (!_filePath)
			{
				DEBUG_LOG_ERROR("Shaderのファイルパスがnullです\n");
				return nullptr;
			}
			DEBUG_LOG_ERROR("ファイルが見つかりません : filePath = {}\n", TextEncoding::ToUtf8(_filePath));
			return nullptr;
		}

		// エラー時はerrorBolbにメッセージが入るため取り出す
		if (errorBlob)
		{
			const char* errorMessage{ static_cast<const char*>(errorBlob->GetBufferPointer()) }; // バッファの先頭アドレスを取得し文字列に変換する
			std::string errorStr{}; // エラー文字列格納用
			errorStr.assign(errorMessage, errorBlob->GetBufferSize()); // 先頭から文字列のサイズ分だけ再代入する
			errorStr += "\n"; // 改行
			DEBUG_LOG_ERROR("{}", errorStr.c_str()); // char型を取り出し出力
		}
		else
		{
			// コンパイルに失敗したメッセージを出力
			DEBUG_LOG_ERROR("Shaderをコンパイルできませんでした\n");
		}
		return nullptr; // 失敗ならnullを返す
	}

	return compiledShader; // コンパイルされたShaderのオブジェクトを返す
}

ComPtr<ID3D12PipelineState> PipelineFactory::BuildComputePipeline(ID3D12Device* _device, ID3DBlob* _cs, ID3D12RootSignature* _rootSignature)
{	
	if (!_device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return nullptr;
	}

	if (!_cs || !_rootSignature)
	{
		DEBUG_LOG_ERROR("ComputeShaderまたはRootSignatureが不正です\n");
		return nullptr;
	}

	// コンパイル済みシェーダーのバイトコードを取り出す
	const void* shaderBytecode{ _cs->GetBufferPointer() };
	const SIZE_T shaderSize{ _cs->GetBufferSize() };

	if (!shaderBytecode || shaderSize == 0)
	{
		DEBUG_LOG_ERROR("ComputeShaderのバイトコードが空です\n");
		return nullptr;
	}

	// CSとRootSignatureを組み合わせてPSOの設定を作る
	D3D12_COMPUTE_PIPELINE_STATE_DESC nativeDesc{};
	nativeDesc.pRootSignature = _rootSignature;
	nativeDesc.CS.pShaderBytecode = shaderBytecode;
	nativeDesc.CS.BytecodeLength = shaderSize;
	nativeDesc.NodeMask = 0; // 単一GPUなので0
	nativeDesc.CachedPSO = {}; // PSOキャッシュは現在使用しない
	nativeDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ComPtr<ID3D12PipelineState> pipeline{};
	HRESULT result{ _device->CreateComputePipelineState(&nativeDesc, IID_PPV_ARGS(&pipeline)) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("ComputePipelineStateの作成に失敗しました\n");
		return nullptr;
	}
	
	return pipeline;
}

ComPtr<ID3D12PipelineState> PipelineFactory::BuildGraphicsPipeline(ID3D12Device* _device, ID3D12RootSignature* _rootSig, const GraphicsPipelineDesc& _desc, const D3D12_SHADER_BYTECODE& _vs, const D3D12_SHADER_BYTECODE& _ps, const D3D12_SHADER_BYTECODE& _hs, const D3D12_SHADER_BYTECODE& _ds, const D3D12_SHADER_BYTECODE& _gs)
{
	if (!_device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return nullptr;
	}

	// RootSigは共有するので外部からうけとる
	if (!_rootSig)
	{
		DEBUG_LOG_ERROR("渡されたRootSignatureが不正です\n");
		return nullptr;
	}

	// GraphicsPipelineではVSを必須とする
	if (!_vs.pShaderBytecode || _vs.BytecodeLength == 0)
	{
		DEBUG_LOG_ERROR("GraphicsPipelineにVSが設定されていません\n");
		return nullptr;
	}

	// IDを取り出す
	const size_t layoutID{ static_cast<size_t>(_desc.layout) };
	const size_t blendID{ static_cast<size_t>(_desc.blend) };
	const size_t depthID{ static_cast<size_t>(_desc.depth) };
	const size_t colorTargetID{ static_cast<size_t>(_desc.colorTarget) };

	// 各enumがテーブルの範囲内にあるか確認する
	if (layoutID >= static_cast<size_t>(InputLayout::Count) ||
		blendID >= static_cast<size_t>(BlendMode::Count) ||
		depthID >= static_cast<size_t>(DepthParam::Count) ||
		colorTargetID >= static_cast<size_t>(ColorTargetParam::Count))
	{
		DEBUG_LOG_ERROR("GraphicsPipelineDescに無効なIDが指定されています\n");
		return nullptr;
	}

	const bool hasHS{ _hs.BytecodeLength > 0 && _hs.pShaderBytecode != nullptr };
	const bool hasDS{ _ds.BytecodeLength > 0 && _ds.pShaderBytecode != nullptr };
	// テッセレーションではHSとDSはセットで扱う
	if (hasHS != hasDS)
	{
		DEBUG_LOG_ERROR("HSとDSは両方設定する必要があります\n");
		return nullptr;
	}
	if (hasHS && _desc.topology != D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
	{
		DEBUG_LOG_ERROR("HSとDSを使用する場合はTopologyTypeをPATCHにしてください\n");
		return nullptr;
	}
	if (!hasHS && _desc.topology == D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
	{
		DEBUG_LOG_ERROR("PATCHを使用する場合はHSとDSが必要です\n");
		return nullptr;
	}

	// PSOの実際のDescを組み立てる
	D3D12_GRAPHICS_PIPELINE_STATE_DESC nativeDesc{};
	nativeDesc.pRootSignature = _rootSig;
	nativeDesc.VS = _vs;
	nativeDesc.PS = _ps;
	nativeDesc.HS = _hs;
	nativeDesc.DS = _ds;
	nativeDesc.GS = _gs;


	// InputLayoutはTableを使う
	const LayoutEntry& layout{ LAYOUT_TABLE[layoutID] };
	nativeDesc.InputLayout.pInputElementDescs = layout.elements; // noneだとnull
	nativeDesc.InputLayout.NumElements = layout.count; // noneだと0

	// Blend設定
	const D3D12_RENDER_TARGET_BLEND_DESC& blend{ BLEND_TABLE[blendID] };
	nativeDesc.BlendState.AlphaToCoverageEnable = false; // αテスト無し
	nativeDesc.BlendState.IndependentBlendEnable = false; // それぞれのパイプラインステートに対して個別のブレンドステートを割り当てない
	nativeDesc.BlendState.RenderTarget[0] = blend;

	// 深度ステートとDSVフォーマットは組で選択する
	const DepthEntry& depth{ DEPTH_TABLE[depthID] };
	nativeDesc.DepthStencilState = depth.state;
	nativeDesc.DSVFormat = depth.dsvFormat;

	// ラスタライザ設定
	nativeDesc.RasterizerState.FillMode = _desc.fillMode; // 塗るかwireか
	nativeDesc.RasterizerState.CullMode = _desc.cullMode;
	nativeDesc.RasterizerState.FrontCounterClockwise = _desc.frontCounterClockwise; // ポリゴンのどちらを表とするか
	nativeDesc.RasterizerState.DepthBias = _desc.depthBias;
	nativeDesc.RasterizerState.DepthBiasClamp = _desc.depthBiasClamp;
	nativeDesc.RasterizerState.SlopeScaledDepthBias = _desc.slopeScaledDepthBias;

	// 深度範囲外の頂点をクリップする
	nativeDesc.RasterizerState.DepthClipEnable = true;
	nativeDesc.RasterizerState.MultisampleEnable = false;
	nativeDesc.RasterizerState.AntialiasedLineEnable = false;
	nativeDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// 残りの設定
	nativeDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	nativeDesc.PrimitiveTopologyType = _desc.topology;

	// RenderTarget関連
	const ColorTargetEntry& colorTarget{ COLOR_TARGET_TABLE[colorTargetID] };
	nativeDesc.NumRenderTargets = colorTarget.count;
	nativeDesc.RTVFormats[0] = colorTarget.format;

	// MSAAなし
	nativeDesc.SampleDesc.Count = 1;
	nativeDesc.SampleDesc.Quality = 0;


	// 生成して登録
	ComPtr<ID3D12PipelineState> pipeline{};
	HRESULT result{ _device->CreateGraphicsPipelineState(&nativeDesc, IID_PPV_ARGS(&pipeline)) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("パイプラインステートの作成に失敗しました。\n");
		return nullptr;
	}

	return pipeline;
}

ComPtr<ID3D12RootSignature> PipelineFactory::BuildGraphicsRootSignature(ID3D12Device* _device, const RootSignatureDesc& _desc)
{
	if (!_device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return nullptr;
	}

	// 独自のRootSignatureDescをD3D12用にする
	std::vector<D3D12_ROOT_PARAMETER> nativeParams(_desc.parameters.size());

	// DescriptorTableごとのRange配列
	std::vector<std::vector<D3D12_DESCRIPTOR_RANGE>> nativeRangeStorage(_desc.parameters.size());

	// パラメータをひとつづつ変換する
	for (size_t i = 0; i < _desc.parameters.size(); i++)
	{
		const RootParamDesc& src{ _desc.parameters[i] }; // パラメータを取り出す

		D3D12_ROOT_PARAMETER& dst{ nativeParams[i] }; // パラメータを取り出す
		dst = {}; // unionが含まれるのでまず0初期化する

		// RootParameterの種類と参照可能なShaderStageは共通
		dst.ParameterType = src.type;
		dst.ShaderVisibility = src.visibility;

		switch (src.type)
		{
		case D3D12_ROOT_PARAMETER_TYPE_CBV:
		case D3D12_ROOT_PARAMETER_TYPE_SRV:
		case D3D12_ROOT_PARAMETER_TYPE_UAV:
			// RootDescriptorの場合処理 cbvならb番号、srvならt番号、uavならu番号
			dst.Descriptor.ShaderRegister = src.shaderRegister;
			dst.Descriptor.RegisterSpace = src.registerSpace;
			break;

		case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
			// RootConstantの場合
			dst.Constants.ShaderRegister = src.shaderRegister;
			dst.Constants.RegisterSpace = src.registerSpace;
			dst.Constants.Num32BitValues = src.num32BitValues;

			// 要素数0のRootConstantsは意味がない
			if (src.num32BitValues == 0)
			{
				DEBUG_LOG_ERROR("RootConstantの要素数が0でした\n");
				return nullptr;
			}
			break;

		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
		{
			// DescriptorTableには最低1つのRangeが必要
			if (src.ranges.empty())
			{
				DEBUG_LOG_ERROR("DescriptorTableにRangeがありません\n");
				return nullptr;
			}

			std::vector<D3D12_DESCRIPTOR_RANGE>& ranges{ nativeRangeStorage[i] };
			ranges.reserve(src.ranges.size()); // push_backよる再確保を避ける


			for (const DescriptorRangeDesc& rangeSrc : src.ranges)
			{
				D3D12_DESCRIPTOR_RANGE range{};

				// SRV/CBV/UAV/Samplerのどれを並べるか
				range.RangeType = rangeSrc.type;
				// Descriptorを連続して何個並べるか
				range.NumDescriptors = rangeSrc.numDescriptors;
				// 開始番号
				range.BaseShaderRegister = rangeSrc.baseShaderRegister;
				// 通常space0
				range.RegisterSpace = rangeSrc.registerSpace;
				// Table内の配置位置
				range.OffsetInDescriptorsFromTableStart = rangeSrc.offset;

				// 個数0のRangeは無効
				if (range.NumDescriptors == 0)
				{
					DEBUG_LOG_ERROR("Rangeの個数が0でした\n");
					return nullptr;
				}

				ranges.push_back(range); // データを詰め込む
			}
			dst.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
			// rangesの全要素を追加し終わってからdata()を取得する 先に取得すると再確保によるポインタ無効を引き起こす可能性がある
			dst.DescriptorTable.pDescriptorRanges = ranges.data();
			break;
		}
		default:
			DEBUG_LOG_ERROR("未対応のRootParameterTypeです\n");
			return nullptr;
		}
	}
	D3D12_ROOT_SIGNATURE_DESC nativeDesc{};
	// IAの入力レイアウトを使用可能にするなどのフラグ
	nativeDesc.Flags = _desc.flags;

	// RootParameter配列を渡す
	nativeDesc.NumParameters = static_cast<UINT>(nativeParams.size());
	nativeDesc.pParameters = nativeParams.empty() ? nullptr : nativeParams.data(); // パラメータが空かチェックする
	// Static Samplerは独自Desc側でD3D12型を直接保有しているのでそのままポインタを渡す
	nativeDesc.NumStaticSamplers = static_cast<UINT>(_desc.staticSamplers.size());
	nativeDesc.pStaticSamplers = _desc.staticSamplers.empty() ? nullptr : _desc.staticSamplers.data();
	// _descは関数終了まで存在するのでdataはserialize中有効
	ComPtr<ID3DBlob> signatureBlob{};
	ComPtr<ID3DBlob> errorBlob{};

	// serialize(RootSignatureの設計図をGPUドライバが扱えるバイナリに変換する)
	HRESULT result{ D3D12SerializeRootSignature(&nativeDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("RootSignatureのserialize化に失敗しました\n");
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			OutputDebugStringA("\n");
		}
		return nullptr;
	}
	// 実際にRootSingatureを作る
	ComPtr<ID3D12RootSignature> rootSignature{}; // 失敗してもComPtrなので自動解放してくれる
	result = _device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("RootSingatureの作成に失敗しました\n");
		return nullptr;
	}

	return rootSignature;
}

ComPtr<ID3D12RootSignature> PipelineFactory::BuildComputeRootSignature(ID3D12Device* _device)
{
	if (!_device)
	{
		DEBUG_LOG_ERROR("デバイスが設定されていません\n");
		return nullptr;
	}

	// 現在のDescriptorManagerは連続して配置する仕組みではないのでひとつづつ入れたTabelを4つ用意してそれぞれの位置に渡すようにする
	// 各スロットはDescriptor一つ分のTableとして定義する
	D3D12_DESCRIPTOR_RANGE inputRanges[MAX_COMPUTE_SRV_COUNT];
	D3D12_DESCRIPTOR_RANGE outputRanges[MAX_COMPUTE_UAV_COUNT];

	D3D12_ROOT_PARAMETER parameters[static_cast<UINT>(ComputeRootParameter::Count)];

	// 定数バッファはRootDescriptorを用いて仮想GPUアドレスを直接渡す
	D3D12_ROOT_PARAMETER& constantBufferParam{ parameters[static_cast<UINT>(ComputeRootParameter::ConstantBuffer)] };
	constantBufferParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	constantBufferParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL; // ComputeなのでALL
	constantBufferParam.Descriptor.ShaderRegister = 0; // b0
	constantBufferParam.Descriptor.RegisterSpace = 0;

	// 入力をt0から順番に割り当てる
	for (std::size_t i = 0; i < MAX_COMPUTE_SRV_COUNT; i++)
	{
		D3D12_DESCRIPTOR_RANGE& range{ inputRanges[i] };
		range.NumDescriptors = 1; // 一つづつバインドする
		range.BaseShaderRegister = static_cast<UINT>(i);
		range.RegisterSpace = 0;
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		const UINT parameterIndex{ static_cast<UINT>(ComputeRootParameter::InputBuffer) + static_cast<UINT>(i) }; // 先頭に現在のIndexを足す

		D3D12_ROOT_PARAMETER& parameter{ parameters[parameterIndex] };
		parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameter.DescriptorTable.NumDescriptorRanges = 1;
		parameter.DescriptorTable.pDescriptorRanges = &range;
	}

	// 出力をu0から順番に割り当てる
	for (std::size_t i = 0; i < MAX_COMPUTE_UAV_COUNT; i++)
	{
		D3D12_DESCRIPTOR_RANGE& range{ outputRanges[i] };
		range.NumDescriptors = 1; // 一つづつバインドする
		range.BaseShaderRegister = static_cast<UINT>(i);
		range.RegisterSpace = 0;
		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
		range.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
		const UINT parameterIndex{ static_cast<UINT>(ComputeRootParameter::OutputBuffer) + static_cast<UINT>(i) }; // 先頭に現在のIndexを足す

		D3D12_ROOT_PARAMETER& parameter{ parameters[parameterIndex] };
		parameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
		parameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
		parameter.DescriptorTable.NumDescriptorRanges = 1;
		parameter.DescriptorTable.pDescriptorRanges = &range;
	}

	// Computeは頂点入力を使わないのでIA用フラグは付けない
	D3D12_ROOT_SIGNATURE_DESC nativeDesc{};
	nativeDesc.NumParameters = static_cast<UINT>(ComputeRootParameter::Count);
	nativeDesc.pParameters = parameters;
	nativeDesc.NumStaticSamplers = 0;
	nativeDesc.pStaticSamplers = nullptr;
	nativeDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ComPtr<ID3DBlob> signatureBlob{};
	ComPtr<ID3DBlob> errorBlob{};

	// RootSignatureの設計図(Desc類)をバイナリ化
	HRESULT result{ D3D12SerializeRootSignature(&nativeDesc, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("Compute用RootSignatureのserialize化に失敗しました\n");
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			OutputDebugStringA("\n");
		}
		return nullptr;
	}

	// バイナリからRootSignatureの作成
	ComPtr<ID3D12RootSignature> rootSignature{};
	result = _device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("RootSignatureの作成に失敗しました\n");
		return nullptr;
	}
	return rootSignature;
}
