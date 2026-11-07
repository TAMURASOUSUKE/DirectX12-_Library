#include <algorithm>
#include <string>
#include <utility>
#include "PipelineFactory.h"
#include "../Debug/DebugLogs.h"
#include "ShaderSystem.h"

// ビルド時にFXCが生成する内蔵Shaderバイトコード
#include "ShapeVSBytecode.h"
#include "ShapePSBytecode.h"

#include "ModelVSBytecode.h"
#include "ModelPSBytecode.h"
#include "ModelShadowVSBytecode.h"

#include "TextureVSBytecode.h"
#include "TexturePSBytecode.h"

#include "TerrainVSBytecode.h"
#include "TerrainHSBytecode.h"
#include "TerrainDSBytecode.h"
#include "TerrainPSBytecode.h"

#include "PostEffectVSBytecode.h"
#include "PostEffectPSBytecode.h"

#include "Primitive3DVSBytecode.h"
#include "Primitive3DUnlitPSBytecode.h"
#include "Primitive3DLitPSBytecode.h"

#include "VFXVSBytecode.h"

namespace {
	// 共通部品作成ヘルパー関数
	// CBV作成
	RootParamDesc MakeRootCBV(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility, UINT _registerSpace = 0)
	{
		RootParamDesc desc{};
		desc.type = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファに設定
		desc.shaderRegister = _shaderRegister;
		desc.registerSpace = 0;
		desc.visibility = _visibility;
		desc.registerSpace = _registerSpace; // レジスタ番号がユーザー定義と内蔵定義でかぶらないようにするための論理的名前空間
		return desc;
	}

	// バッファをGPU仮想アドレスから直接読むRootSRVの作成
	RootParamDesc MakeRootSRV(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility)
	{
		RootParamDesc desc{};
		desc.type = D3D12_ROOT_PARAMETER_TYPE_SRV;
		desc.shaderRegister = _shaderRegister;
		desc.registerSpace = 0;
		desc.visibility = _visibility;
		return desc;
	}

	// DescriptorTableでのSRV作成
	RootParamDesc MakeSRVTable(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility, UINT _registerSpace = 0)
	{
		RootParamDesc desc{};
		desc.type = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタ―テーブル指定
		desc.visibility = _visibility;

		DescriptorRangeDesc range{}; // レンジ設定
		range.type = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRV指定
		range.numDescriptors = 1;
		range.baseShaderRegister = _shaderRegister;
		range.registerSpace = _registerSpace;
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

	// ID3DBlobが持つコンパイル済みデータをD3D12がPSO作成時に受け取る形式に変換
	D3D12_SHADER_BYTECODE MakeShaderBytecode(ID3DBlob* _blob)
	{
		// 使用しないShaderStageは空のバイトコードを返す
		if (!_blob) return {};
		D3D12_SHADER_BYTECODE bytecode{};
		bytecode.pShaderBytecode = _blob->GetBufferPointer();
		bytecode.BytecodeLength = _blob->GetBufferSize();
		return bytecode;
	}

	// Releaseで埋め込んだバイト配列をD3D12形式へ変換する
	D3D12_SHADER_BYTECODE MakeShaderBytecode(const void* _data, SIZE_T _size)
	{
		if (!_data || _size == 0) return {};
		D3D12_SHADER_BYTECODE bytecode{};
		bytecode.pShaderBytecode = _data;
		bytecode.BytecodeLength = _size;
		return bytecode;
	}

	// BuiltinShaderIDと同じ順番で、ビルド時生成済みバイトコードを並べる
	const D3D12_SHADER_BYTECODE BUILTIN_SHADER_BYTECODE_TABLE[]
	{
		MakeShaderBytecode(g_ShapeVS, sizeof(g_ShapeVS)),
		MakeShaderBytecode(g_ShapePS, sizeof(g_ShapePS)),

		MakeShaderBytecode(g_Primitive3DVS, sizeof(g_Primitive3DVS)),
		MakeShaderBytecode(g_Primitive3DUnlitPS, sizeof(g_Primitive3DUnlitPS)),
		MakeShaderBytecode(g_Primitive3DLitPS, sizeof(g_Primitive3DLitPS)),

		MakeShaderBytecode(g_ModelVS, sizeof(g_ModelVS)),
		MakeShaderBytecode(g_ModelPS, sizeof(g_ModelPS)),
		MakeShaderBytecode(g_ModelShadowVS, sizeof(g_ModelShadowVS)),

		MakeShaderBytecode(g_TextureVS, sizeof(g_TextureVS)),
		MakeShaderBytecode(g_TexturePS, sizeof(g_TexturePS)),

		MakeShaderBytecode(g_TerrainVS, sizeof(g_TerrainVS)),
		MakeShaderBytecode(g_TerrainHS, sizeof(g_TerrainHS)),
		MakeShaderBytecode(g_TerrainDS, sizeof(g_TerrainDS)),
		MakeShaderBytecode(g_TerrainPS, sizeof(g_TerrainPS)),

		MakeShaderBytecode(g_PostEffectVS, sizeof(g_PostEffectVS)),
		MakeShaderBytecode(g_PostEffectPS, sizeof(g_PostEffectPS)),

		MakeShaderBytecode(g_VFXVS, sizeof(g_VFXVS)),
	};

	// ID追加時にバイトコード表の追加忘れを検出する
	static_assert(std::size(BUILTIN_SHADER_BYTECODE_TABLE) == static_cast<size_t>(BuiltinShaderID::Count));

	// ShadowMapの保存深度と現在地点の深度を比較するサンプラー
	D3D12_STATIC_SAMPLER_DESC MakeShadowComparisonSampler(UINT _shaderRegister, D3D12_SHADER_VISIBILITY _visibility)
	{
		D3D12_STATIC_SAMPLER_DESC desc{};

		// 周囲の深度比較結果を線形補完する簡易PCF
		desc.Filter = D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;

		// ShadowMap範囲外は深度1.0として扱い、影にしない
		desc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		desc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		desc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
		desc.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;

		desc.MipLODBias = 0.0f;
		desc.MaxAnisotropy = 1;

		// 現在地点の深度がShadoMap深度以下なら光が届いている
		desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

		// ShadowMapはMipMapを持たない
		desc.MinLOD = 0.0f;
		desc.MaxLOD = 0.0f;

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

bool ShaderSystem::RegisterBuiltinRootSignature(const RootSignatureDesc& _desc)
{
	if (!device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return false;
	}

	// 範囲外かつ無効値(Count)を見る
	const size_t id{ static_cast<size_t>(_desc.rootSignatureID) }; // IDを数値化
	if (id >= static_cast<size_t>(RootSigID::Count))
	{
		DEBUG_LOG_ERROR("RootSignatureが範囲外でした\n");
		return false;
	}

	// 生成して登録
	ComPtr<ID3D12RootSignature> rootSig{ PipelineFactory::BuildGraphicsRootSignature(device, _desc) };
	if (!rootSig)
	{
		DEBUG_LOG_ERROR("RootSignature生成に失敗しました\n");
		return false;
	}
	rootSigs[id] = rootSig;
	return true;
}

bool ShaderSystem::RegisterBuiltinPipeline(const GraphicsPipelineDesc& _desc)
{
	if (!device)
	{
		DEBUG_LOG_ERROR("Deviceが設定されていません\n");
		return false;
	}

	const size_t pipelineID{ static_cast<size_t>(_desc.pipelineID) }; // IDを取り出す
	const size_t rootSigID{ static_cast<size_t>(_desc.rootSignatureID) }; 
	const size_t layoutID{ static_cast<size_t>(_desc.layout) };
	const size_t blendID{ static_cast<size_t>(_desc.blend) };
	const size_t depthID{ static_cast<size_t>(_desc.depth) };
	const size_t colorTargetID{ static_cast<size_t>(_desc.colorTarget) };

	// 各enumがテーブルの範囲内にあるか確認する
	if (pipelineID >= static_cast<size_t>(PipelineID::Count) ||
		rootSigID >= static_cast<size_t>(RootSigID::Count) || 
		layoutID >= static_cast<size_t>(InputLayout::Count) ||
		blendID >= static_cast<size_t>(BlendMode::Count) ||
		depthID >= static_cast<size_t>(DepthParam::Count) ||
		colorTargetID >= static_cast<size_t>(ColorTargetParam::Count))
	{
		DEBUG_LOG_ERROR("GraphicsPipelineDescに無効なIDが指定されています\n");
		return false;
	}

	if (pipelines[pipelineID])
	{
		DEBUG_LOG_ERROR(
			"同じPipelineIDのPSOが既に登録されています\n"
		);
		return false;
	}

	// RootSignatureが先に作られているか確認
	if (!rootSigs[rootSigID])
	{
		DEBUG_LOG_ERROR("指定されたRootSignatureが作成されていません\n");
		return false;
	}

	// VSは必須とする
	if (_desc.vs == BuiltinShaderID::None)
	{
		DEBUG_LOG_ERROR("VSが設定されていません\n");
		return false;
	}

	// テッセレーションではHSとDSはセットで扱う
	const bool usesHS{ _desc.hs != BuiltinShaderID::None };
	const bool usesDS{ _desc.ds != BuiltinShaderID::None };
	if (usesHS != usesDS)
	{
		DEBUG_LOG_ERROR("HSとDSは両方設定する必要があります\n");
		return false;
	}
	if (usesHS && _desc.topology != D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
	{
		DEBUG_LOG_ERROR("HSとDSを使用する場合はTopologyTypeをPATCHにしてください\n");
		return false;
	}
	if (!usesHS && _desc.topology == D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH)
	{
		DEBUG_LOG_ERROR("PATCHを使用する場合はHSとDSが必要です\n");
		return false;
	}

	// IDからコンパイル済みBytecodeを取得する
	const D3D12_SHADER_BYTECODE vs{ GetBuiltinShaderBytecode(_desc.vs) };
	const D3D12_SHADER_BYTECODE ps{ GetBuiltinShaderBytecode(_desc.ps) };
	const D3D12_SHADER_BYTECODE hs{ GetBuiltinShaderBytecode(_desc.hs) };
	const D3D12_SHADER_BYTECODE ds{ GetBuiltinShaderBytecode(_desc.ds) };
	const D3D12_SHADER_BYTECODE gs{ GetBuiltinShaderBytecode(_desc.gs) };

	// 必須VSの取得失敗
	if (!vs.pShaderBytecode || vs.BytecodeLength == 0)
	{
		DEBUG_LOG_ERROR("内蔵VSの取得に失敗しました\n");
		return false;
	}

	// 指定した任意ステージが取得できなかった場合
	if (_desc.ps != BuiltinShaderID::None && (!ps.pShaderBytecode || ps.BytecodeLength == 0))
	{
		DEBUG_LOG_ERROR("内蔵PSの取得に失敗しました\n");
		return false;
	}
	if (usesHS && (!hs.pShaderBytecode || hs.BytecodeLength == 0))
	{
		DEBUG_LOG_ERROR("内蔵HSの取得に失敗しました\n");
		return false;
	}
	if (usesDS && (!ds.pShaderBytecode || ds.BytecodeLength == 0))
	{
		DEBUG_LOG_ERROR("内蔵DSの取得に失敗しました\n");
		return false;
	}
	if (_desc.gs != BuiltinShaderID::None && (!gs.pShaderBytecode || gs.BytecodeLength == 0))
	{
		DEBUG_LOG_ERROR("内蔵GSの取得に失敗しました\n");
		return false;
	}

	// 生成して登録
	ComPtr<ID3D12PipelineState> pipeline{ PipelineFactory::BuildGraphicsPipeline(device, rootSigs[rootSigID].Get(), _desc, vs, ps, hs, ds, gs)};

	if (!pipeline)
	{
		DEBUG_LOG_ERROR("GraphicsPipelineの構築に失敗しました\n");
		return false;
	}

	// 台帳へ登録
	pipelines[pipelineID] = pipeline;
	return true;
}

ComPtr<ID3D12PipelineState> ShaderSystem::CreateMaterialPipeline(ShaderUsage _usage, ID3DBlob* _vertexShader, ID3DBlob* _pixelShader, const MaterialRenderState& _renderState)
{
	if (!device)
	{
		DEBUG_LOG_ERROR("デバイスが設定されていません\n");
		return nullptr;
	}

	if (!_pixelShader)
	{
		DEBUG_LOG("Materialに使用するPixelShaderがnullです\n");
		return nullptr;
	}

	// 頂点シェーダーが入力されいていない場合は内蔵のものを使う
	BuiltinShaderID defaultVertexShader{ BuiltinShaderID::None };
	GraphicsPipelineDesc desc{};
	// 指定されたAlphaModeからBlendとDepthを設定する(Spriteは深度を持たせないので引数で指定)
	const auto choosBlendAndDepth
	{
		[&desc, &_renderState](bool _isSprite)
		{
			// Spriteの状態ではdepthの設定は必要ないので省略する
			desc.depth = DepthParam::None; // 最初にNoneにしておく
			switch (_renderState.alphaMode)
			{
			case MaterialAlphaMode::Opaque:
			case MaterialAlphaMode::Mask:
				desc.blend = BlendMode::Opaque;
				if(!_isSprite) desc.depth = DepthParam::ReadWrite;
				break;
			case MaterialAlphaMode::Blend:
				desc.blend = BlendMode::Alpha;
				if (!_isSprite) desc.depth = DepthParam::ReadOnly;
				break;
			case MaterialAlphaMode::Additive:
				desc.blend = BlendMode::Additive;
				if (!_isSprite) desc.depth = DepthParam::ReadOnly;
				break;
				default:
					DEBUG_LOG_ERROR("不正な状態が渡されました\n");
					return false;
			}
			return true;
		}
	};

	switch (_usage)
	{
	case ShaderUsage::PostEffect:
		// 内蔵のVSを指定
		defaultVertexShader = BuiltinShaderID::PostEffectVS;
		// PostEffect用の標準PSO設定
		desc.rootSignatureID = RootSigID::PostEffect;
		// 動的なMaterialなので固定PipelineIDは使用しない
		desc.pipelineID = PipelineID::Count;
		// SV_VertexIDからフルスクリーン三角形を作るため頂点バッファのLayoutは必要ない
		desc.layout = InputLayout::None;
		// バックバッファ全体を書き換えるためブレンドなし
		desc.blend = BlendMode::Opaque;
		// 画面全体への後処理なので深度は使用しない
		desc.depth = DepthParam::None;
		desc.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.fillMode = D3D12_FILL_MODE_SOLID;
		break;

	case ShaderUsage::Sprite:
		// 内蔵のVSを指定
		defaultVertexShader = BuiltinShaderID::TextureVS;
		// Sprite用の標準PSO設定
		desc.rootSignatureID = RootSigID::Texture;
		// 動的なMaterialなので固定PipelineIDは使用しない
		desc.pipelineID = PipelineID::Count;
		desc.layout = InputLayout::Sprite;
		desc.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.fillMode = D3D12_FILL_MODE_SOLID;
		if (!choosBlendAndDepth(true)) return nullptr; // Spriteなのでblendだけ設定
		break;
	case ShaderUsage::Model:
		defaultVertexShader = BuiltinShaderID::ModelVS;
		desc.rootSignatureID = RootSigID::Model;
		desc.pipelineID = PipelineID::Count;
		desc.layout = InputLayout::Model;
		desc.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.fillMode = D3D12_FILL_MODE_SOLID;
		desc.cullMode = _renderState.doubleSided ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK; // 設定されていれば両面そうでなければ背面カリング
		if (!choosBlendAndDepth(false)) return nullptr; // blendとdepthを条件によって設定する
		break;
	case ShaderUsage::VFX:
		defaultVertexShader = BuiltinShaderID::VFXVS;
		desc.rootSignatureID = RootSigID::VFX;
		desc.pipelineID = PipelineID::Count;
		desc.layout = InputLayout::VFX;
		desc.topology = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		desc.fillMode = D3D12_FILL_MODE_SOLID;
		desc.cullMode = _renderState.doubleSided ? D3D12_CULL_MODE_NONE : D3D12_CULL_MODE_BACK; // 設定されていれば両面そうでなければ背面カリング
		if (!choosBlendAndDepth(false)) return nullptr; // blendとdepthを条件によって設定する
		break;
	default:
		DEBUG_LOG_ERROR("不明なShaderUsageです\n");
		return nullptr;
	}

	D3D12_SHADER_BYTECODE vertexBytecode{};
	// ユーザーがVSを指定している場合はそのコンパイル結果を使う
	if (_vertexShader) vertexBytecode = MakeShaderBytecode(_vertexShader);
	else vertexBytecode = GetBuiltinShaderBytecode(defaultVertexShader);

	if (!vertexBytecode.pShaderBytecode || vertexBytecode.BytecodeLength == 0)
	{
		DEBUG_LOG_ERROR("Mateiral用VertexShaderの取得に失敗しました\n");
		return nullptr;
	}
	const D3D12_SHADER_BYTECODE pixelBytecode{ MakeShaderBytecode(_pixelShader) };

	return PipelineFactory::BuildGraphicsPipeline(device, rootSigs[static_cast<size_t>(desc.rootSignatureID)].Get(), desc, vertexBytecode, pixelBytecode);
}

std::vector<RootSignatureDesc> ShaderSystem::MakeRootSignatureDescs() const
{
	std::vector<RootSignatureDesc> descs{};
	// SpriteRootSignature
	RootSignatureDesc texture{};
	texture.rootSignatureID = RootSigID::Texture;
	texture.parameters.push_back(MakeSRVTable(0, D3D12_SHADER_VISIBILITY_PIXEL)); // rootParamの0番目にはテクスチャ(t0)
	texture.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_VERTEX)); // rootParamの1番目には座標変換用(b0)
	// RootParam[2] - [5] : materailSlot0-3 HLSL側ではb0-b3
	for (UINT i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
	{
		texture.parameters.push_back(MakeRootCBV(MATERIAL_PARAMETER_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
	// ユーザーテクスチャを追加する RootParam[6] - [9] HLSL側ではt0-t3
	for (UINT i = 0; i < MATERIAL_RESOURCE_PARAMETER_SLOT_COUNT; i++)
	{
		texture.parameters.push_back(MakeSRVTable(MATERIAL_TEXTURE_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
	texture.staticSamplers.push_back(MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_PIXEL)); // staticSampler0番目(s0)
	descs.push_back(std::move(texture)); // texture変数は使わないのでmoveして空にする(コピーの必要性なし)

	// ModelRootSignature
	RootSignatureDesc model{};
	model.rootSignatureID = RootSigID::Model;
	model.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_ALL)); // 座標変換などの定数バッファ 今後PSからもカメラ位置を読むため(b0)
	model.parameters.push_back(MakeRootCBV(1, D3D12_SHADER_VISIBILITY_PIXEL)); // material用定数バッファ(b1)
	model.parameters.push_back(MakeRootCBV(2, D3D12_SHADER_VISIBILITY_VERTEX)); // スキニング行列(b2)
	model.parameters.push_back(MakeSRVTable(0, D3D12_SHADER_VISIBILITY_PIXEL)); // テクスチャ(t0)
	model.parameters.push_back(MakeRootCBV(3, D3D12_SHADER_VISIBILITY_PIXEL)); // ライティング(b3)
	model.parameters.push_back(MakeRootCBV(4, D3D12_SHADER_VISIBILITY_VERTEX)); // モデル個体ごとのデータ(b4)
	model.staticSamplers.push_back(MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_PIXEL)); // サンプラー設定(s0)
	model.parameters.push_back(MakeSRVTable(1, D3D12_SHADER_VISIBILITY_PIXEL)); // MetallicRoughnessテクスチャ(t1)
	model.parameters.push_back(MakeRootCBV(5, D3D12_SHADER_VISIBILITY_ALL)); // Shadow用LightViewProjection(b5)
	model.parameters.push_back(MakeSRVTable(2, D3D12_SHADER_VISIBILITY_PIXEL)); // ShadowMapのt2
	for (UINT i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
	{
		// RootParam[9]-[12]へmaterial slot0-3を追加する
		 // ユーザーが定義した定数バッファを受け取る 内蔵と番号の重複に耐えるためにregisterSpaceを1に設定
		model.parameters.push_back(MakeRootCBV(MATERIAL_PARAMETER_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
	model.staticSamplers.push_back(MakeShadowComparisonSampler(1, D3D12_SHADER_VISIBILITY_PIXEL)); // ShadowMap比較用のSampler(s1)
	// ユーザーテクスチャを追加する RootParam[13] - [16] HLSL側ではt0-t3
	for (UINT i = 0; i < MATERIAL_RESOURCE_PARAMETER_SLOT_COUNT; i++)
	{
		model.parameters.push_back(MakeSRVTable(MATERIAL_TEXTURE_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
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
	RootSignatureDesc postEffect{};
	postEffect.rootSignatureID = RootSigID::PostEffect;
	postEffect.parameters.push_back(MakeSRVTable(0, D3D12_SHADER_VISIBILITY_PIXEL)); // シーンRTのSRVをt0としてピクセルシェーダーから読む
	for (UINT i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
	{
		// RootParam[1]-[4]へmaterial slot0-3を追加する
		 // ユーザーが定義した定数バッファを受け取る 内蔵と番号の重複に耐えるためにregisterSpaceを1に設定
		postEffect.parameters.push_back(MakeRootCBV(MATERIAL_PARAMETER_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
	// ユーザーテクスチャを追加する RootParam[5] - [8] HLSL側ではt0-t3
	for (UINT i = 0; i < MATERIAL_RESOURCE_PARAMETER_SLOT_COUNT; i++)
	{
		postEffect.parameters.push_back(MakeSRVTable(MATERIAL_TEXTURE_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
	D3D12_STATIC_SAMPLER_DESC sampler{ MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_PIXEL) };
	// 画面端で反対側のピクセルを拾わないようにClampする
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	postEffect.staticSamplers.push_back(sampler);
	postEffect.flags = D3D12_ROOT_SIGNATURE_FLAG_NONE; // 頂点バッファを使用しないためIAの許可は不要
	descs.push_back(std::move(postEffect));
	// インスタンシング対応3D基礎図形
	RootSignatureDesc primitive3D{};
	primitive3D.rootSignatureID = RootSigID::Primitive3D;
	// 全図形共通のViewProjectionをb0からVSが読む
	primitive3D.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_VERTEX)); // RootParameter[0]
	// 個体ごとのWorld行列、法線行列、色をt0からVSが読む
	primitive3D.parameters.push_back(MakeRootSRV(0, D3D12_SHADER_VISIBILITY_VERTEX)); // RootParameter[1]
	primitive3D.parameters.push_back(MakeRootCBV(3, D3D12_SHADER_VISIBILITY_PIXEL)); // b3(ライティング)
	descs.push_back(std::move(primitive3D));
	// VFX
	RootSignatureDesc vfxDesc{};
	vfxDesc.rootSignatureID = RootSigID::VFX;
	vfxDesc.parameters.push_back(MakeRootCBV(0, D3D12_SHADER_VISIBILITY_VERTEX)); // RootParameter[0]
	for (UINT i = 0; i < MATERIAL_PARAMETER_SLOT_COUNT; i++)
	{
		 // RootParam[1]-[4]へmaterial slot0-3を追加する
		 // ユーザーが定義した定数バッファを受け取る 内蔵と番号の重複に耐えるためにregisterSpaceを1に設定
		vfxDesc.parameters.push_back(MakeRootCBV(MATERIAL_PARAMETER_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
	// ユーザーテクスチャを追加する RootParam[5] - [8] HLSL側ではt0-t3
	for (UINT i = 0; i < MATERIAL_RESOURCE_PARAMETER_SLOT_COUNT; i++)
	{
		vfxDesc.parameters.push_back(MakeSRVTable(MATERIAL_TEXTURE_REGISTER_BASE + i, D3D12_SHADER_VISIBILITY_ALL, USER_DEFINE_REGISTER_SPACE_NUM));
	}
	vfxDesc.staticSamplers.push_back(MakeLinearWrapSampler(0, D3D12_SHADER_VISIBILITY_PIXEL)); // staticSampler0番目(s0)
	descs.push_back(std::move(vfxDesc));
	return descs;
}

D3D12_SHADER_BYTECODE ShaderSystem::GetBuiltinShaderBytecode(BuiltinShaderID _id)
{
	if (_id == BuiltinShaderID::None) return {}; // このステージを使わないなら空

	const size_t index{ static_cast<size_t>(_id) };
	if (index >= static_cast<size_t>(BuiltinShaderID::Count))
	{
		DEBUG_LOG_ERROR("無効なBuiltinShaderIDが指定されました ID = {}\n", index);
		return {};
	}

	return BUILTIN_SHADER_BYTECODE_TABLE[index];
}
