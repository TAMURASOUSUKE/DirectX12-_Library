#include <cmath>
#include "../Core/Handle/ModelHandle.h"
#include "../Component/Transform.h"
#include "GraphicsType.h"
#include "ShaderSystem.h"
#include "LightSystem.h"
#include "CameraSystem.h"
#include "../Math/TSMath.h"
#include "../Debug/DebugLogs.h"
#include "GraphicsResourceManager.h"
#include "GraphicsDevice.h"
#include "DescriptorManager.h"
#include "ModelRenderer.h"

namespace
{
	// 1フレーム内のモデル描画で共有するデータ
	struct SceneFrameCB
	{
		Mat4x4 viewProjection{ Mat4x4::Identity };
		Vector4 cameraPosition{}; // wは未使用
	};
	// モデル個体ごとに異なるデータ
	struct ModelObjectCB
	{
		Mat4x4 world{ Mat4x4::Identity };
		Mat4x4 worldInverseTranspose{ Mat4x4::Identity }; // Worldの逆転置行列を作ることで非均一スケールでも正しく法線を取れるようにする
	};
	static_assert(sizeof(SceneFrameCB) == 80, "SceneFrameCBのサイズがHLSLと一致しません");
	static_assert(sizeof(ModelObjectCB) == 128, "ModelObjectCBのサイズがHLSLと一致しません");
}

namespace
{
	// 静的モデルとアニメーションモデルで個体それぞれのCBを組み立てるヘルパー
	bool TryMakeModelObjectCB(const Transform& _transform, ModelObjectCB& _outCB)
	{
		const Vector3 scale{ _transform.GetScale() };

		// 逆数を計算するため0スケールは受け入れない
		if (std::abs(scale.x) <= Math::EPSILON || std::abs(scale.y) <= Math::EPSILON || std::abs(scale.z) <= Math::EPSILON)
		{
			DEBUG_LOG_ERROR("ModelのScaleに0へ近い値が指定されました Scale : ({}, {}, {})", scale.x, scale.y, scale.z);
			return false;
		}
		const Vector3 inverseScale{ 1.0f / scale.x, 1.0f / scale.y, 1.0f / scale.z };
		_outCB.world = _transform.GetWorldMatrix();

		// TransformをSRT構成にしたので逆行列計算を使わずに逆スケールと回転行列を使って法線行列を作る
		_outCB.worldInverseTranspose = Mat4x4::MakeScaling(inverseScale) * _transform.GetRotation().ToMat4x4();
		return true;
	}
}

bool ModelRenderer::Setup(ShaderSystem* _shaderSystem, CameraSystem* _cameraSystem, LightSystem* _lightSystem)
{
	if (!_shaderSystem || !_cameraSystem || !_lightSystem)
	{
		DEBUG_LOG_ERROR("ModelRendererに渡されるシステムが不正です\n");
		return false;
	}
	shaderSystem = _shaderSystem;
	cameraSystem = _cameraSystem;
	lightSystem = _lightSystem;

	sceneFrameRingCBV.Setup(static_cast<UINT>(sizeof(SceneFrameCB)), 1); // 1フレームに一回更新なので指定する
	modelObjectRingCBV.Setup(static_cast<UINT>(sizeof(ModelObjectCB)));
	skinningRingCBV.Setup(sizeof(Mat4x4) * MAX_BONE_NUM);
	materialRingCBV.Setup(sizeof(MaterialCB));

	return true;
}

void ModelRenderer::Shutdown()
{
	sceneFrameRingCBV.Shutdown();
	modelObjectRingCBV.Shutdown();
	materialRingCBV.Shutdown();
	skinningRingCBV.Shutdown();
}

void ModelRenderer::BeginFrame()
{
	sceneFrameRingCBV.Reset();
	modelObjectRingCBV.Reset();
	materialRingCBV.Reset();
	skinningRingCBV.Reset();
	sceneFrameGPUAddress = 0; // 更新するため0
}

bool ModelRenderer::PrepareModelData(const Transform& _transform, const AnimInstanceData* _animation, PreparedModelDrawData& _outData)
{
	// まず結果を返す構造体を空にする
	_outData = {};
	ModelObjectCB objectData{};
	if (!TryMakeModelObjectCB(_transform, objectData)) return false;

	// モデル個体のGPUAddressの取得
	const D3D12_GPU_VIRTUAL_ADDRESS objectAdress{ modelObjectRingCBV.Update(&objectData, static_cast<UINT>(sizeof(objectData))) };
	//スキニングのアドレス
	D3D12_GPU_VIRTUAL_ADDRESS skinningAddress{ 0 };
	if (_animation)
	{
		// サイズと空確認
		if (_animation->skinningMatrices.empty() || _animation->skinningMatrices.size() > MAX_BONE_NUM)
		{
			DEBUG_LOG_ERROR("スキニング行列が不正です Count : {}\n", _animation->skinningMatrices.size());
			return false;
		}
		skinningAddress = skinningRingCBV.Update(_animation->skinningMatrices.data(), static_cast<UINT>(sizeof(Mat4x4) * _animation->skinningMatrices.size()));
	}
	else
	{
		// アニメーションがない場合(静的モデル)
		skinningAddress = skinningRingCBV.Update(&Mat4x4::Identity, static_cast<UINT>(sizeof(Mat4x4)));
	}

	if (objectAdress == 0 || skinningAddress == 0)
	{
		DEBUG_LOG_ERROR("モデル個体データのGPU転送に失敗しました\n");
		return false;
	}
	_outData.objectAddress = objectAdress;
	_outData.skinningAddress = skinningAddress;

	return true;
}

bool ModelRenderer::BeginModelDraw(D3D12_GPU_VIRTUAL_ADDRESS _shadowFrameAddress, D3D12_GPU_DESCRIPTOR_HANDLE _shadowMapSRV)
{
	auto cmd{ GraphicsDevice::Instance().GetCommandList() };

	// VPとカメラ位置をGPUへ転送したアドレスの取得
	const D3D12_GPU_VIRTUAL_ADDRESS frameAddress{ GetSceneFrameGPUAddress() };

	// 平行光源と環境光は全モデルで共通するのでLightSystemが用意した今のフレームのアドレスを使う
	const D3D12_GPU_VIRTUAL_ADDRESS lightAddress{ lightSystem->GetFrameGPUAddress() };
	if (frameAddress == 0 || lightAddress == 0 || _shadowFrameAddress == 0 || _shadowMapSRV.ptr == 0)
	{
		DEBUG_LOG_ERROR("モデル描画の共通データ取得に失敗しました\n");
		return false;
	}

	// RootParamの構成をGPUへ指定
	cmd->SetGraphicsRootSignature(shaderSystem->GetRootSignature(RootSigID::Model));
	DescriptorManager::Instance().SetDiscriptor(cmd); // DescriptorHealをCommandListへ設定
	cmd->SetGraphicsRootConstantBufferView(0, frameAddress); // カメラ位置やVP(b0)
	cmd->SetGraphicsRootConstantBufferView(4, lightAddress); // ライティング計算(b3)
	cmd->SetGraphicsRootConstantBufferView(7, _shadowFrameAddress); // b5
	cmd->SetGraphicsRootDescriptorTable(8, _shadowMapSRV); // t2

	// gltfモデルのIndexBufferは3頂点ごとの三角形として扱う。
	cmd->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	return true;

}

bool ModelRenderer::DrawSubMesh(const ModelDrawPacket& _packet)
{
	if (!_packet.subMesh || _packet.pipelineID == PipelineID::Count || _packet.preparedData.objectAddress == 0 || _packet.preparedData.skinningAddress == 0)
	{
		DEBUG_LOG_ERROR("ModelDrawPacketの内容が不正です\n");
		return false;
	}
	auto cmd{ GraphicsDevice::Instance().GetCommandList() };

	// 名前を読みやすくするための参照
	const SubMesh& subMesh{ *_packet.subMesh };
	ID3D12PipelineState* pipeline{ shaderSystem->GetPipeline(_packet.pipelineID) };
	if (!pipeline)
	{
		DEBUG_LOG_ERROR("モデル用Pipelineの取得に失敗しました\n");
		return false;
	}
	cmd->SetPipelineState(pipeline);

	// 静的モデルの場合は単位行列1個
	cmd->SetGraphicsRootConstantBufferView(2, _packet.preparedData.skinningAddress); // (b2)
	// World行列と法線用の逆転置行列
	cmd->SetGraphicsRootConstantBufferView(5, _packet.preparedData.objectAddress); // (b4) 

	// materialはサブメッシュごとに転送
	MaterialCB materialCB{};
	materialCB.baseColorFactor = subMesh.material.baseColorFactor;
	materialCB.metallic = subMesh.material.metallic;
	materialCB.roughness = subMesh.material.roughness;
	materialCB.alphaMode = static_cast<std::uint32_t>(subMesh.material.alphaMode); // 32bit値へ変換を掛ける
	materialCB.alphaCutoff = subMesh.material.alphaCutoff;

	// 現在フレームから未使用スライスの取得->MaterialCBのコピー
	const D3D12_GPU_VIRTUAL_ADDRESS materialAddress{ materialRingCBV.Update(&materialCB, static_cast<UINT>(sizeof(materialCB))) };
	if (materialAddress == 0)
	{
		DEBUG_LOG_ERROR("materialCBのGPU転送に失敗しました\n");
		return false;
	}

	cmd->SetGraphicsRootConstantBufferView(1, materialAddress); // (b1)

	// BaseColor
	TextureData* baseColorTexture{ GraphicsResourceManager::Instance().Lookup(subMesh.material.textures[MaterialTex::BaseColor])}; // (t0)
	if (!baseColorTexture)
	{
		// 欠落等が起こった場合にはエラーテクスチャ
		baseColorTexture = GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetErrorTexture());
	}
	if (!baseColorTexture)
	{
		// エラーテクスチャも失敗したらログ
		DEBUG_LOG_ERROR("BaseColorとErrorTextureの取得に失敗しました\n");
		return false;
	}
	cmd->SetGraphicsRootDescriptorTable(3, baseColorTexture->srvHandle.gpu); // (t0)

	// Roughnessはt1 glTFではG = Roughness B = Metallicとして使う
	const TexHandle metallicRoughnessHandle{ subMesh.material.textures[MaterialTex::MetallicRoughness] };
	TextureData* metallicRoughnessTexture{ nullptr };

	// ラフネスが有効な場合に読み込む
	if (metallicRoughnessHandle.IsValid()) metallicRoughnessTexture = GraphicsResourceManager::Instance().Lookup(metallicRoughnessHandle);
	if (!metallicRoughnessTexture)
	{
		// MetallicRoughnessがない場合はデフォルトが読み込まれる MetallicもBaseColor同様エラーテクスチャを使うとわかりにくいのでデフォルトの白色を使う
		metallicRoughnessTexture = GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetDefaultTexture());
	}
	cmd->SetGraphicsRootDescriptorTable(6, metallicRoughnessTexture->srvHandle.gpu); // (t1)

	cmd->IASetVertexBuffers(0, 1, &subMesh.vertexBuffer.vertexView); // InputAssemblerへ登録
	cmd->IASetIndexBuffer(&subMesh.indexBuffer.indexView); // 頂点Indexを登録(読み込み時点で右手系から左手系にしている)

	cmd->DrawIndexedInstanced(subMesh.indexBuffer.indexCount, 1, 0, 0, 0); // IndexBufferの要素数分描画
	return true;
}

bool ModelRenderer::BeginShadowDraw(D3D12_GPU_VIRTUAL_ADDRESS _shadowFrameAddress)
{
	if (!shaderSystem || _shadowFrameAddress == 0)
	{
		DEBUG_LOG_ERROR("Shadowモデル描画に必要な共通データが不正です\n");
		return false;
	}
	auto cmd{ GraphicsDevice::Instance().GetCommandList() };
	if (!cmd)
	{
		DEBUG_LOG_ERROR("Shadowモデル描画用CommandListを取得できません\n");
		return false;
	}

	ID3D12PipelineState* shadowPipeline{ shaderSystem->GetPipeline(PipelineID::ModelShadow) };
	if (!shadowPipeline)
	{
		DEBUG_LOG_ERROR("ModelShadow用Pipelineを取得できません\n");
		return false;
	}
	// ModelShadowVSは通常Modelと同じb0・b2・b4を使用するため現段階では既存のModel用RootSignatureを共有する
	cmd->SetGraphicsRootSignature(shaderSystem->GetRootSignature(RootSigID::Model));

	// ShadowPass中は全サブメッシュで同じPSOを使用する
	cmd->SetPipelineState(shadowPipeline);

	// RootParameter[0]にはHLSLのShadowFrameCB、つまりb0を設定する
	cmd->SetGraphicsRootConstantBufferView(0, _shadowFrameAddress);

	// glTFモデルのIndexBufferは三角形リストとして描画する
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return true;
}

bool ModelRenderer::DrawShadowSubMesh(const ModelDrawPacket& _packet)
{
	if (!_packet.subMesh || _packet.preparedData.objectAddress == 0 || _packet.preparedData.skinningAddress == 0)
	{
		DEBUG_LOG_ERROR("Shadow描画用ModelDrawPacketが不正です\n");
		return false;
	}
	auto cmd{ GraphicsDevice::Instance().GetCommandList() };
	if (!cmd)
	{
		DEBUG_LOG_ERROR("Shadowモデル描画用CommandListを取得できません\n");
		return false;
	}
	const SubMesh& subMesh{ *_packet.subMesh };
	// RootParameter[2]はModelShadowVSのBoneCB : register(b2)
	cmd->SetGraphicsRootConstantBufferView(2, _packet.preparedData.skinningAddress);
	// RootParameter[5]はModelObjectCB : register(b4)
	cmd->SetGraphicsRootConstantBufferView(5, _packet.preparedData.objectAddress);
	cmd->IASetVertexBuffers(0, 1, &subMesh.vertexBuffer.vertexView);
	cmd->IASetIndexBuffer(&subMesh.indexBuffer.indexView);
	cmd->DrawIndexedInstanced(subMesh.indexBuffer.indexCount, 1, 0, 0, 0);
	return true;
}

D3D12_GPU_VIRTUAL_ADDRESS ModelRenderer::GetSceneFrameGPUAddress()
{
	// 現在フレームですでに転送していれば再利用
	if (sceneFrameGPUAddress != 0) return sceneFrameGPUAddress;

	const Vector3 cameraPosition{ cameraSystem->GetCameraPosition() };
	SceneFrameCB frameData{};
	frameData.viewProjection = cameraSystem->GetViewProjectionMatrix();
	frameData.cameraPosition = { cameraPosition.x, cameraPosition.y, cameraPosition.z, 0.0f };

	sceneFrameGPUAddress = sceneFrameRingCBV.Update(&frameData, static_cast<UINT>(sizeof(frameData)));
	if (sceneFrameGPUAddress == 0) DEBUG_LOG_ERROR("SceneFrameCBのGPU転送に失敗しました\n");
	return sceneFrameGPUAddress;
}
