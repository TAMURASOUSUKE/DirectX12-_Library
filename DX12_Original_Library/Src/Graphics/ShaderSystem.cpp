#include <algorithm>
#include <string>
#include <d3dcompiler.h>
#include "../Debug/DebugLogs.h"
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
}

namespace
{
	// 共通部品作成ヘルパー関数
	// CBV作成
	RootParamDesc MakeRootCBV(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility)
	{
		RootParamDesc desc{};
		desc.type = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファに設定
		desc.shaderRegister = _shaderRegister;
		desc.registerSpace = 0;
		desc.visibility = _visibility;
		return desc;
	}

	// DescriptorTableでのSRV作成
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
		desc.MaxAnisotropy = 1; // 異方性フィルタリングの倍率(現在は異方性フィルターではないため実質未使用)
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
void ShaderSystem::Setup(ID3D12Device* _device)
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
	// PSOはRootSigを使って作成しているため先にPSOを解放する
	for (ComPtr<ID3D12PipelineState>& pipeline : pipelines)
	{
		pipeline.Reset();
	}

	// PipelineState解放の後にRootSigを解放する
	for (ComPtr<ID3D12RootSignature>& rootSig : rootSigs)
	{
		rootSig.Reset();
	}
	// deviceをnull化して今後使わないようにする
	device = nullptr;
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
	if (!device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return false;
	}

	const size_t id{ static_cast<size_t>(_desc.rootSignatureID) }; // IDを数値化
	// 範囲外かつ無効値(Count)を見る
	if (id >= static_cast<size_t>(RootSigID::Count))
	{
		DEBUG_LOG_ERROR("RootSignatureが範囲外でした\n");
		return false;
	}

	// 既に使われているIDを上書きすると、
	// Batchが保持している生ポインタが無効になる可能性がある
	if (rootSigs[id])
	{
		DEBUG_LOG_ERROR("同じRootSigIDのRootSignatureが既に登録されています\n");
		return false;
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
				return false;
			}
			break;

		case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
		{
			// DescriptorTableには最低1つのRangeが必要
			if (src.ranges.empty())
			{
				DEBUG_LOG_ERROR("DescriptorTableにRangeがありません\n");
				return false;
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
					return false;
				}

				ranges.push_back(range); // データを詰め込む
			}
			dst.DescriptorTable.NumDescriptorRanges = static_cast<UINT>(ranges.size());
			// rangesの全要素を追加し終わってからdata()を取得する
			// 先に取得すると、再確保によるポインタ無効を引き起こす可能性がある
			dst.DescriptorTable.pDescriptorRanges = ranges.data();
			break;
		}
		default:
			DEBUG_LOG_ERROR("未対応のRootParameterTypeです\n");
			return false;
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
	HRESULT result{D3D12SerializeRootSignature(&nativeDesc, D3D_ROOT_SIGNATURE_VERSION_1_0, &signatureBlob, &errorBlob)};
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("RootSignatureのserialize化に失敗しました\n");
		if (errorBlob)
		{
			OutputDebugStringA(static_cast<const char*>(errorBlob->GetBufferPointer()));
			OutputDebugStringA("\n");
		}
		return false;
	}
	// 実際にRootSingatureを作る
	ComPtr<ID3D12RootSignature> rootSignature{}; // 失敗してもComPtrなので自動解放してくれる
	result = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("RootSingatureの作成に失敗しました\n");
		return false;
	}
	// すべて成功した状態で登録を行う
	rootSigs[id] = rootSignature;

	return true;
}

bool ShaderSystem::CreateGraphicsPipeline(const GraphicsPipelineDesc& _desc)
{
	if (!device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return false;
	}

	const size_t pipelineID{static_cast<size_t>(_desc.pipelineID)}; // IDを取り出す
	const size_t rootSignatureID{ static_cast<size_t>(_desc.rootSignatureID) };
	const size_t layoutID{ static_cast<size_t>(_desc.layout) };
	const size_t blendID{ static_cast<size_t>(_desc.blend) };
	const size_t depthID{ static_cast<size_t>(_desc.depth) };

	// 各enumがテーブルの範囲内にあるか確認する
	if (pipelineID >= static_cast<size_t>(PipelineID::Count) ||
		rootSignatureID >= static_cast<size_t>(RootSigID::Count) ||
		layoutID >= static_cast<size_t>(InputLayout::Count) ||
		blendID >= static_cast<size_t>(BlendMode::Count) ||
		depthID >= static_cast<size_t>(DepthParam::Count))
	{
		DEBUG_LOG_ERROR("GraphicsPipelineDescに無効なIDが指定されています\n");
		return false;
	}

	// RootSignatureが先に作られているか確認
	if (!rootSigs[rootSignatureID])
	{
		DEBUG_LOG_ERROR("指定されたRootSignatureが作成されていません\n");
		return false;
	}

	if (pipelines[pipelineID])
	{
		DEBUG_LOG_ERROR(
			"同じPipelineIDのPSOが既に登録されています\n"
		);
		return false;
	}

	// VSは必須とする
	if (!_desc.vsPath)
	{
		DEBUG_LOG_ERROR("VSパスが設定されていません\n");
		return false;
	}

	// 各シェーダーのコンパイル
	ComPtr<ID3DBlob> vsBlob{ Compile(_desc.vsPath, "main", "vs_5_0") }; // 頂点
	if (!vsBlob) { DEBUG_LOG_ERROR("VSのコンパイルに失敗しました\n"); return false; }

	ComPtr<ID3DBlob> psBlob{}; // ピクセル
	ComPtr<ID3DBlob> hsBlob{}; // ハル
	ComPtr<ID3DBlob> dsBlob{}; // ドメイン
	ComPtr<ID3DBlob> gsBlob{}; // ジオメトリ

	// テッセレーションではHSとDSはセットで扱う
	if ((_desc.hsPath == nullptr) != (_desc.dsPath == nullptr))
	{
		DEBUG_LOG_ERROR("HSとDSは両方設定する必要があります\n");
		return false;
	}
	if ((_desc.hsPath != nullptr) && _desc.topology != D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
	{
		DEBUG_LOG_ERROR("HSとDSを使用する場合はTopologyTypeをPATCHにしてください\n");
		return false;
	}
	if ((_desc.hsPath == nullptr) && _desc.topology == D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
	{
		DEBUG_LOG_ERROR("PATCHを使用する場合はHSとDSが必要です\n");
		return false;
	}

	if (_desc.psPath)
	{
		psBlob = Compile(_desc.psPath, "main", "ps_5_0");
		if (!psBlob) { DEBUG_LOG_ERROR("PSのコンパイルに失敗しました\n"); return false; }
	}
	if (_desc.hsPath)
	{
		hsBlob = Compile(_desc.hsPath, "main", "hs_5_0"); // ハル
		if (!hsBlob) { DEBUG_LOG_ERROR("HSのコンパイルに失敗しました\n"); return false; }
	}
	if (_desc.dsPath)
	{
		dsBlob = Compile(_desc.dsPath, "main", "ds_5_0"); // ドメイン
		if (!dsBlob) { DEBUG_LOG_ERROR("DSのコンパイルに失敗しました\n"); return false; }
	}
	if (_desc.gsPath)
	{
		gsBlob = Compile(_desc.gsPath, "main", "gs_5_0"); // ジオメトリ
		if (!gsBlob) { DEBUG_LOG_ERROR("GSのコンパイルに失敗しました\n"); return false; }
	}

	// PSOの実際のDescを組み立てる
	D3D12_GRAPHICS_PIPELINE_STATE_DESC nativeDesc{};
	// PSO対応するRootSingatureを出す
	nativeDesc.pRootSignature = rootSigs[rootSignatureID].Get();

	// shader群(現状VSは必須としているのでif無し)
	nativeDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer();
	nativeDesc.VS.BytecodeLength = vsBlob->GetBufferSize();

	if (psBlob)
	{
		nativeDesc.PS.pShaderBytecode = psBlob->GetBufferPointer();
		nativeDesc.PS.BytecodeLength = psBlob->GetBufferSize();
	}
	if (hsBlob)
	{
		nativeDesc.HS.pShaderBytecode = hsBlob->GetBufferPointer();
		nativeDesc.HS.BytecodeLength = hsBlob->GetBufferSize();
	}
	if (dsBlob)
	{
		nativeDesc.DS.pShaderBytecode = dsBlob->GetBufferPointer();
		nativeDesc.DS.BytecodeLength = dsBlob->GetBufferSize();
	}
	if (gsBlob)
	{
		nativeDesc.GS.pShaderBytecode = gsBlob->GetBufferPointer();
		nativeDesc.GS.BytecodeLength = gsBlob->GetBufferSize();
	}
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
	nativeDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // 一旦全てNone(今後モデル等では拡張する可能性あり)
	nativeDesc.RasterizerState.FrontCounterClockwise = false;

	// 深度範囲外の頂点をクリップする
	nativeDesc.RasterizerState.DepthClipEnable = true;
	nativeDesc.RasterizerState.MultisampleEnable = false;
	nativeDesc.RasterizerState.AntialiasedLineEnable = false;
	nativeDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// 残りの設定
	nativeDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	nativeDesc.PrimitiveTopologyType = _desc.topology;

	// とりあえず今はRenderTargetを1枚だけ使用する
	nativeDesc.NumRenderTargets = 1;
	nativeDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	// MSAAなし
	nativeDesc.SampleDesc.Count = 1;
	nativeDesc.SampleDesc.Quality = 0;

	// 生成して登録
	ComPtr<ID3D12PipelineState> pipeline{};
	HRESULT result{ device->CreateGraphicsPipelineState(&nativeDesc, IID_PPV_ARGS(&pipeline)) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("パイプラインステートの作成に失敗しました。\n");
		return false;
	}

	// 台帳へ登録
	pipelines[pipelineID] = pipeline;
	return true;
}

bool ShaderSystem::CreateComputePipeline(const ComputePipelineDesc& _desc)
{
	if (!device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return false;
	}

	const size_t pipelineID{ static_cast<size_t>(_desc.pipelineID) }; // IDを取り出す
	const size_t rootSignatureID{ static_cast<size_t>(_desc.rootSignatureID) };

	// 台帳配列の範囲外になっていないか確認する
	if (pipelineID >= static_cast<size_t>(PipelineID::Count) ||
		rootSignatureID >= static_cast<size_t>(RootSigID::Count))
	{
		DEBUG_LOG_ERROR("ComputePipelineDescに無効なIDが指定されています\n");
		return false;
	}

	// RootSignatureはPipelineより先に作成されている必要がある
	if (!rootSigs[rootSignatureID])
	{
		DEBUG_LOG_ERROR("指定されたCompute用RootSignatureが作成されていません\n");
		return false;
	}

	// CSは必須
	if (!_desc.csPath)
	{
		DEBUG_LOG_ERROR("CSのパスが設定されていません\n");
		return false;
	}

	// 同じIDを上書きすると外部で保持している生ポインタが無効になる可能性があるため禁止する
	if (pipelines[pipelineID])
	{
		DEBUG_LOG_ERROR("同じPipelineIDのPSOが既に登録されています\n");
		return false;
	}

	ComPtr<ID3DBlob> csBlob{ Compile(_desc.csPath, "main", "cs_5_0") };
	if (!csBlob)
	{
		DEBUG_LOG_ERROR("CSのコンパイルに失敗しました\n");
		return false;
	}

	// ComputePipeline用のD3D12ネイティブDescを作る
	D3D12_COMPUTE_PIPELINE_STATE_DESC nativeDesc{};
	nativeDesc.pRootSignature = rootSigs[rootSignatureID].Get();
	nativeDesc.CS.pShaderBytecode = csBlob->GetBufferPointer();
	nativeDesc.CS.BytecodeLength = csBlob->GetBufferSize();
	nativeDesc.NodeMask = 0; // 複数GPUを明示的に扱わないので0
	nativeDesc.CachedPSO = {}; // 既存PSOキャッシュは現在使用しない
	nativeDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	ComPtr<ID3D12PipelineState> pipeline{};
	HRESULT result{ device->CreateComputePipelineState(&nativeDesc, IID_PPV_ARGS(&pipeline)) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("ComputePipelineStateの作成に失敗しました\n");
		return false;
	}

	// 台帳への記録
	pipelines[pipelineID] = pipeline;

	return true;
}

std::vector<RootSignatureDesc> ShaderSystem::MakeRootSignatureDescs() const
{
	std::vector<RootSignatureDesc> descs{};
	// SpriteRootSignature
	RootSignatureDesc texture{};
	texture.rootSignatureID = RootSigID::Texture;
	texture.parameters.push_back(MakeSRVTable(0, D3D12_SHADER_VISIBILITY_PIXEL)); // rootParamの0番目にはテクスチャ(t0)
	texture.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_VERTEX)); // rootParamの1番目には座標変換用(b0)
	texture.staticSamplers.push_back(MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_PIXEL)); // staticSampler0番目(s0)
	descs.push_back(std::move(texture)); // texture変数は使わないのでmoveして空にする(コピーの必要性なし)
	// ModelRootSignature
	RootSignatureDesc model{};
	model.rootSignatureID = RootSigID::Model;
	model.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_VERTEX)); // 座標変換などの定数バッファ(b0)
	model.parameters.push_back(MakeRootCBV(1, D3D12_SHADER_VISIBILITY_PIXEL)); // material用定数バッファ(b1)
	model.parameters.push_back(MakeRootCBV(2, D3D12_SHADER_VISIBILITY_VERTEX)); // スキニング行列(b2)
	model.parameters.push_back(MakeSRVTable(0, D3D12_SHADER_VISIBILITY_PIXEL)); // テクスチャ(t0)
	model.staticSamplers.push_back(MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_PIXEL)); // サンプラー設定(s0)
	descs.push_back(std::move(model)); // model変数は使わないのでmoveして空にする(コピーの必要性なし)
	// Shape用
	RootSignatureDesc shape{};
	shape.rootSignatureID = RootSigID::Shape;
	shape.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_VERTEX)); // 座標変換(b0)
	descs.push_back(std::move(shape)); // shape変数は使わないのでmoveして空にする(コピーの必要性なし)
	// Terrain用
	RootSignatureDesc terrain{};
	terrain.rootSignatureID = RootSigID::Terrain;
	terrain.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_HULL)); // HS用のCBV
	terrain.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_DOMAIN)); // DS用のCBV 番号が同じでもステージが違うから共存できる
	terrain.parameters.push_back(MakeSRVTable(0, D3D12_SHADER_VISIBILITY_DOMAIN)); // DS用のテクスチャ
	// サンプラー
	D3D12_STATIC_SAMPLER_DESC terrainSampler{ MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_DOMAIN) }; // 基本的なサンプラー設定
	// UV範囲外では端のピクセルにする
	terrainSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	terrainSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	terrainSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	terrain.staticSamplers.push_back(terrainSampler);
	descs.push_back(std::move(terrain)); // shape変数は使わないのでmoveして空にする(コピーの必要性なし)
	// PostEffect
	RootSignatureDesc postEffectDesc{};
	postEffectDesc.rootSignatureID = RootSigID::PostEffect;
	postEffectDesc.parameters.push_back(MakeSRVTable(0, D3D12_SHADER_VISIBILITY_PIXEL)); // シーンRTのSRVをt0としてピクセルシェーダーから読む
	D3D12_STATIC_SAMPLER_DESC sampler{MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_PIXEL)};
	// 画面端で反対側のピクセルを拾わないようにClampする
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	postEffectDesc.staticSamplers.push_back(sampler);
	postEffectDesc.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE; // 頂点バッファを使用しないためIAの許可は不要
	descs.push_back(std::move(postEffectDesc));
	return descs;
}
