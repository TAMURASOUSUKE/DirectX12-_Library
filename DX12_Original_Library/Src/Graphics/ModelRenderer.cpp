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

void ModelRenderer::DrawSkinnedModel(const AnimInstanceData& _anim, const Transform& _transform)
{
	// skinningRingCBVはMAX_BONE_NUM個分しか確保していないため GPUへ送る前に上限を確認する
	if (_anim.skinningMatrices.size() > MAX_BONE_NUM)
	{
		DEBUG_LOG_ERROR("モデルのボーン数が上限を超えています ""boneCount:{} max:{}\n", _anim.skinningMatrices.size(), MAX_BONE_NUM);
		return;
	}

	ModelData* model{ GraphicsResourceManager::Instance().Lookup(_anim.modelHandle) }; // ハンドル分解
	if (!model) return;

	auto cmd{ GraphicsDevice::Instance().GetCommandList() };
	ModelObjectCB objectData{};
	if (!TryMakeModelObjectCB(_transform, objectData)) return;

	cmd->SetGraphicsRootSignature(shaderSystem->GetRootSignature(RootSigID::Model));
	cmd->SetPipelineState(shaderSystem->GetPipeline(PipelineID::Model));

	DescriptorManager::Instance().SetDiscriptor(cmd);
	const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress{ GetSceneFrameGPUAddress() };
	const D3D12_GPU_VIRTUAL_ADDRESS objectAddress{ modelObjectRingCBV.Update(&objectData, static_cast<UINT>(sizeof(objectData))) };
	const D3D12_GPU_VIRTUAL_ADDRESS skinningAddress{ skinningRingCBV.Update(_anim.skinningMatrices.data(), sizeof(Mat4x4) * static_cast<UINT>(_anim.skinningMatrices.size())) };
	const D3D12_GPU_VIRTUAL_ADDRESS lightAddress{ lightSystem->GetFrameGPUAddress() };
	if ((frameDataAddress <= 0) || (objectAddress <= 0))
	{
		DEBUG_LOG_ERROR("モデル変換データのGPU転送に失敗しました\n");
		return;
	}
	if (skinningAddress <= 0)
	{
		DEBUG_LOG_ERROR("スキンのUpdateで失敗しました\n");
		return;
	}
	if (lightAddress <= 0)
	{
		DEBUG_LOG_ERROR("モデル用SceneLightの取得に失敗しました\n");
		return;
	}
	cmd->SetGraphicsRootConstantBufferView(0, frameDataAddress); // フレーム共通
	cmd->SetGraphicsRootConstantBufferView(2, skinningAddress); // ボーンを更新
	cmd->SetGraphicsRootConstantBufferView(4, lightAddress); // ライトの更新
	cmd->SetGraphicsRootConstantBufferView(5, objectAddress); // モデル個体データ
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// サブメッシュ分回す
	for (const SubMesh& sub : model->subMeshes)
	{
		// material類の更新
		MaterialCB matCB{};
		matCB.baseColorFactor = sub.material.baseColorFactor;
		matCB.metallic = sub.material.metallic;
		matCB.roughness = sub.material.roughness;
		matCB.emissiveFactor = sub.material.emissiveFactor;
		const D3D12_GPU_VIRTUAL_ADDRESS materialAddress{ materialRingCBV.Update(&matCB, sizeof(MaterialCB)) };
		if (materialAddress <= 0)
		{
			DEBUG_LOG_ERROR("マテリアルringbufferのUpateで失敗しました\n");
			return; // materialのUpdateで失敗したらモデルをあきらめる
		}
		cmd->SetGraphicsRootConstantBufferView(1, materialAddress);

		TextureData* tex{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::BaseColor]) }; // ベースカラー
		TextureData* metalllicRoughness{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::MetallicRoughness]) };
		if (tex)
		{
			cmd->SetGraphicsRootDescriptorTable(3, tex->srvHandle.gpu);
		}
		else
		{
			DEBUG_LOG_ERROR("モデルのLookUpに失敗しました\n");
			TextureData* error{ GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetErrorTexture()) }; // エラーハンドルを分解
			if (!error)
			{
				DEBUG_LOG_ERROR("モデルLookup失敗時にエラー用テクスチャのLookUpに失敗しました\n");
				return;
			}
			cmd->SetGraphicsRootDescriptorTable(3, error->srvHandle.gpu);
		}
		// メタリックラフネスだけバインドする
		if (metalllicRoughness)
		{
			cmd->SetGraphicsRootDescriptorTable(6, metalllicRoughness->srvHandle.gpu);
		}
		else
		{
			DEBUG_LOG_ERROR("ラフネスに失敗しました\n");
			// ラフネスは白色にする(GとBを1にするため。pinkだとBだけ1になってつるつるになる)
			TextureData* error{ GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetDefaultTexture()) }; // エラーハンドルを分解
			if (!error)
			{
				DEBUG_LOG_ERROR("ラフネスLookup失敗時にエラー用テクスチャのLookUpに失敗しました\n");
				return;
			}
			cmd->SetGraphicsRootDescriptorTable(6, error->srvHandle.gpu);
		}

		cmd->IASetVertexBuffers(0, 1, &sub.vertexBuffer.vertexView);
		cmd->IASetIndexBuffer(&sub.indexBuffer.indexView);
		cmd->DrawIndexedInstanced(sub.indexBuffer.indexCount, 1, 0, 0, 0);
	}
}

void ModelRenderer::DrawStaticModel(ModelHandle _model, const Transform& _transform)
{
	ModelData* model{ GraphicsResourceManager::Instance().Lookup(_model) };
	if (!model) return; // 無効ハンドルガード
	auto cmd{ GraphicsDevice::Instance().GetCommandList() }; // コマンドリストのキャッシュ
	ModelObjectCB objectData{};
	if (!TryMakeModelObjectCB(_transform, objectData)) return;

	// パイプライン設定
	cmd->SetGraphicsRootSignature(shaderSystem->GetRootSignature(RootSigID::Model));
	cmd->SetPipelineState(shaderSystem->GetPipeline(PipelineID::Model));

	DescriptorManager::Instance().SetDiscriptor(cmd); // Flushと同じ考え方

	const D3D12_GPU_VIRTUAL_ADDRESS frameDataAddress{ GetSceneFrameGPUAddress() };
	const D3D12_GPU_VIRTUAL_ADDRESS objectAddress{ modelObjectRingCBV.Update(&objectData, static_cast<UINT>(sizeof(objectData))) };
	const D3D12_GPU_VIRTUAL_ADDRESS skinningAddress{ skinningRingCBV.Update(&Mat4x4::Identity, sizeof(Mat4x4)) }; // 静的描画なので単位行列
	const D3D12_GPU_VIRTUAL_ADDRESS lightAddress{ lightSystem->GetFrameGPUAddress() };
	if ((frameDataAddress <= 0) || (objectAddress <= 0))
	{
		DEBUG_LOG_ERROR("モデル変換データのGPU転送に失敗しました\n");
		return;
	}
	if (skinningAddress <= 0)
	{
		DEBUG_LOG_ERROR("スキンのUpdateで失敗しました\n");
		return;
	}
	if (lightAddress <= 0)
	{
		DEBUG_LOG_ERROR("モデル用SceneLightの取得に失敗しました\n");
		return;
	}

	cmd->SetGraphicsRootConstantBufferView(0, frameDataAddress);
	cmd->SetGraphicsRootConstantBufferView(2, skinningAddress);
	cmd->SetGraphicsRootConstantBufferView(4, lightAddress);
	cmd->SetGraphicsRootConstantBufferView(5, objectAddress); // モデル個体データ
	cmd->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);



	// submeshループ
	for (const SubMesh& sub : model->subMeshes)
	{
		// material値をCBにつめる
		MaterialCB matCB{};
		matCB.baseColorFactor = sub.material.baseColorFactor;
		matCB.metallic = sub.material.metallic;
		matCB.roughness = sub.material.roughness;
		matCB.emissiveFactor = sub.material.emissiveFactor;
		const D3D12_GPU_VIRTUAL_ADDRESS materialUpdate{ materialRingCBV.Update(&matCB, sizeof(MaterialCB)) };
		if (materialUpdate <= 0)
		{
			DEBUG_LOG_ERROR("マテリアルringbufferのUpateで失敗しました\n");
			return; // materialのUpdateで失敗したらモデルをあきらめる
		}
		// Ringで送ってb1にバインドする
		cmd->SetGraphicsRootConstantBufferView(1, materialUpdate);

		// テクスチャをバインド
		TextureData* tex{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::BaseColor]) };
		TextureData* metalllicRoughness{ GraphicsResourceManager::Instance().Lookup(sub.material.textures[MaterialTex::MetallicRoughness]) };
		if (tex)
		{
			cmd->SetGraphicsRootDescriptorTable(3, tex->srvHandle.gpu);
		}
		else
		{
			DEBUG_LOG_ERROR("モデルのLookUpに失敗しました\n");
			TextureData* error{ GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetDefaultTexture()) }; // エラーハンドルを分解
			if (!error)
			{
				DEBUG_LOG_ERROR("モデルLookup失敗時にエラー用テクスチャのLookUpに失敗しました\n");
				return;
			}
			cmd->SetGraphicsRootDescriptorTable(3, error->srvHandle.gpu);
		}
		// メタリックラフネスだけバインドする
		if (metalllicRoughness)
		{
			cmd->SetGraphicsRootDescriptorTable(6, metalllicRoughness->srvHandle.gpu);
		}
		else
		{
			DEBUG_LOG_ERROR("ラフネスに失敗しました\n");
			TextureData* error{ GraphicsResourceManager::Instance().Lookup(GraphicsResourceManager::Instance().GetErrorTexture()) }; // エラーハンドルを分解
			if (!error)
			{
				DEBUG_LOG_ERROR("ラフネスLookup失敗時にエラー用テクスチャのLookUpに失敗しました\n");
				return;
			}
			cmd->SetGraphicsRootDescriptorTable(6, error->srvHandle.gpu);
		}

		// 頂点インデックスをバインド
		cmd->IASetVertexBuffers(0, 1, &sub.vertexBuffer.vertexView);
		cmd->IASetIndexBuffer(&sub.indexBuffer.indexView);
		cmd->DrawIndexedInstanced(sub.indexBuffer.indexCount, 1, 0, 0, 0);
	}
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
