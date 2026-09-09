#include <cmath>
#include "DescriptorManager.h"
#include "../Debug/DebugLogs.h"
#include "ShadowSystem.h"

namespace
{
	// チェックをようにするためのヘルパー群

	// Vector3の全成分が有効値か
	bool IsFiniteVector3(const Vector3& _value)
	{
		return std::isfinite(_value.x) && std::isfinite(_value.y) && std::isfinite(_value.z);
	}

	// floatが有効値かつ0より大きいか
	bool IsPositiveFinite(float _value)
	{
		return std::isfinite(_value) && _value > Math::EPSILON;
	}

	// HLSL側のShadowFrameCBと同じメモリ構成にする
	struct alignas(16) ShadowFrameCB
	{
		Mat4x4 lightViewProjection{ Mat4x4::Identity };
	};

	static_assert(sizeof(ShadowFrameCB) == sizeof(Mat4x4), "ShadowFrameCBのサイズがHLSL側と一致しません");
}

bool ShadowSystem::Setup(ID3D12Device* _device, UINT _resolution)
{
	if (!_device)
	{
		DEBUG_LOG_ERROR("ShadowSystemに無効なDeviceが渡されました\n");
		return false;
	}

	if (_resolution == 0 || _resolution > D3D12_REQ_TEXTURE2D_U_OR_V_DIMENSION)
	{
		DEBUG_LOG_ERROR("ShadowMapの解像度が不正です Resolution : {}\n", _resolution);
		return false;
	}

	// 二重Setupによるリソース・Descriptorのリークを防ぐ
	if (device || shadowMap || shadowDSV.IsValid() || shadowSRV.IsValid())
	{
		DEBUG_LOG_ERROR("ShadowSystemはすでにSetupされています\n");
		return false;
	}

	device = _device;

	if (!CreateShadowMap(_resolution))
	{
		// CreateShadowMapの途中まで成功していた場合にも備えて戻す
		Shutdown();
		return false;
	}

	shadowFrameRingCBV.Setup(static_cast<UINT>(sizeof(ShadowFrameCB)), 1); // 平行光源1個文なので1フレームにつき1下位更新でよい
	if (shadowFrameRingCBV.GetCurrentVirtualAddress() == 0)
	{
		DEBUG_LOG_ERROR("Shadow用RingConstantBufferの作成に失敗しました\n");
		// 先に作ったShadowMapとDescriptorもまとめて戻す
		Shutdown();
		return false;
	}

	frameGPUAddress = 0;
	hasValidLightMatrices = false;

	return true;
}

void ShadowSystem::Shutdown()
{
	// ShutdownはGPUの使用完了を待ったのちに行われる
	DescriptorManager& descriptorManager{ DescriptorManager::Instance() };
	descriptorManager.Free(HeapType::CBV_SRV_UAV, shadowSRV);
	descriptorManager.Free(HeapType::DSV, shadowDSV);
	shadowSRV = {};
	shadowDSV = {};

	shadowMap.Reset();
	shadowFrameRingCBV.Shutdown();
	frameGPUAddress = 0;
	hasValidLightMatrices = false;

	resolution = 0;
	resourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	lightViewMatrix = Mat4x4::Identity;
	lightProjectionMatrix = Mat4x4::Identity;
	lightViewProjectionMatrix = Mat4x4::Identity;
	isPassActive = false;

	device = nullptr;
}

void ShadowSystem::BeginFrame()
{
	// 現在のバックバッファで使用するスライスを先頭へ戻す
	shadowFrameRingCBV.Reset();

	// 次に要求されたとき現在フレーム用データを再転送する
	frameGPUAddress = 0;
}

bool ShadowSystem::UpdateDirectionalLightMatrices(const DirectionalLight& _light, const DirectionalShadowSettings& _settings)
{
	if (!IsFiniteVector3(_light.direction) || _light.direction.LengthSquared() <= Math::EPSILON * Math::EPSILON)
	{
		DEBUG_LOG_ERROR("影生成に使用する平行光源の方向が不正です\n");
		return false;
	}
	if (!IsFiniteVector3(_settings.focusPosition))
	{
		DEBUG_LOG_ERROR("生成範囲の中心が不正です\n");
		return false;
	}
	// 正射影の範囲と光源カメラの距離を検証する
	if (!IsPositiveFinite(_settings.lightDistance) || !IsPositiveFinite(_settings.width) || !IsPositiveFinite(_settings.height))
	{
		DEBUG_LOG_ERROR("影生成範囲のサイズまたは光源距離が不正です\n");
		return false;
	}
	// Nearは0以上でFarはNearより大きくなくてはならない
	if (!std::isfinite(_settings.nearClip) || !std::isfinite(_settings.farClip) || _settings.nearClip < 0.0f || _settings.farClip <= _settings.nearClip + Math::EPSILON)
	{
		DEBUG_LOG_ERROR("影生成用NearもしくはFarが不正です Near : {} Far : {}", _settings.nearClip, _settings.farClip);
		return false;
	}
	// FocusPositionは光源カメラからlightDistanceだけ離れている その位置がNear~Farの外にあると影を付けたい中心自体が移らない
	if (_settings.lightDistance <= _settings.nearClip || _settings.lightDistance >= _settings.farClip)
	{
		DEBUG_LOG_ERROR("影生成範囲の中心がNear / Farの外側にあります : LightDistance : {} Near : {} Far : {}", _settings.lightDistance, _settings.nearClip, _settings.farClip);
		return false;
	}

	// 平行光源を単位ベクトルへ
	const Vector3 lightDirection{ Vector3::Normalized(_light.direction) };

	Vector3 temporaryUp{ Vector3::Up };
	const float upParallel{ std::abs(Vector3::Dot(lightDirection, Vector3::Up)) }; // 並行かをみたいので絶対値
	if (upParallel >= 0.999f) temporaryUp = Vector3::Forward; // ほぼ平行ならForwardを仮の上方向として扱う

	// 光源カメラの右方向
	const Vector3 lightRight{ Vector3::Normalized(Vector3::Cross(temporaryUp, lightDirection)) };
	// 右方向と前方向から正確に直交する上方向を作り直す
	const Vector3 lightUp{ Vector3::Cross(lightDirection, lightRight) };

	// ShadowMapの1ピクセルが担当するワールド空間の大きさ
	const float worldUnitsPerTexelX{ _settings.width / static_cast<float>(resolution) };
	const float worldUnitsPerTexelY{ _settings.height / static_cast<float>(resolution) };

	// 注視点が光源カメラの軸上でどこにあるか
	const float focusOnLightX{ Vector3::Dot(_settings.focusPosition, lightRight) }; // 横軸
	const float focusOnLightY{ Vector3::Dot(_settings.focusPosition, lightUp) }; // 縦軸

	// ShadowMapの1ピクセル単位へまとめる
	const float snappedFocusX{ std::round(focusOnLightX / worldUnitsPerTexelX) * worldUnitsPerTexelX };
	const float snappedFocusY{ std::round(focusOnLightY / worldUnitsPerTexelY) * worldUnitsPerTexelY };

	// もとの注視点から丸めによって発生した差だけ移動する
	const Vector3 snappedFocusPosition{ _settings.focusPosition + lightRight * (snappedFocusX - focusOnLightX) + lightUp * (snappedFocusY - focusOnLightY) };

	// 丸めた注視点から、光の逆方向へカメラを配置する
	const Vector3 lightPosition{snappedFocusPosition - lightDirection * _settings.lightDistance};

	// 既存のメンバを壊さないようにローカルでいったん作る
	const Mat4x4 newLightView{ Mat4x4::MakeLookAt(lightPosition, snappedFocusPosition, lightUp) };
	const Mat4x4 newLightProjection{ Mat4x4::MakeOrthGraphic(_settings.width, _settings.height, _settings.nearClip, _settings.farClip) };

	// 行ベクトル規約なので頂点 * View * Projection
	const Mat4x4 newLightViewProjection{ newLightView * newLightProjection };

	// すべての計算が完了してから状態を更新する
	lightViewMatrix = newLightView;
	lightProjectionMatrix = newLightProjection;
	lightViewProjectionMatrix = newLightViewProjection;
	hasValidLightMatrices = true; // GPUへ送ってよい行列が完成したとみる
	return true;
}

bool ShadowSystem::BeginShadowPass(ID3D12GraphicsCommandList* _commandList)
{
	if (!_commandList || !IsReady())
	{
		DEBUG_LOG_ERROR("ShadowPassを開始できません CommandListまたはShadowSystemが無効です\n");
		return false;
	}
	if (isPassActive)
	{
		DEBUG_LOG_ERROR("BeginShadowPassが二重に呼ばれました\n");
		return false;
	}

	// 前フレームではPSが読む状態になっているのでここで書き込める状態にしておく
	if (!TransitionResource(_commandList, D3D12_RESOURCE_STATE_DEPTH_WRITE)) return false; // エラー原因は関数もとに書いてある

	// ShadowMap全体へ描画するためのViewport
	const D3D12_VIEWPORT viewport{0.0f, 0.0f, static_cast<float>(resolution), static_cast<float>(resolution), 0.0f, 1.0f};

	// Viewport内で描画を許可する矩形
	const D3D12_RECT scissorRect{ 0, 0, static_cast<LONG>(resolution), static_cast<LONG>(resolution) };
	_commandList->RSSetViewports(1, &viewport);
	_commandList->RSSetScissorRects(1, &scissorRect);

	const D3D12_CPU_DESCRIPTOR_HANDLE dsv{ shadowDSV.cpu };
	_commandList->OMSetRenderTargets(0, nullptr, FALSE, &dsv); // ShadowPassでは色を描画しないためRTVは0個
	_commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr); // 前フレームの深度を消去 (Farを表す1.0fで初期化)
	isPassActive = true;
	return true;
}

bool ShadowSystem::EndShadowPass(ID3D12GraphicsCommandList* _commandList)
{
	if (!_commandList || !IsReady())
	{
		DEBUG_LOG_ERROR("ShadowPassを終了できません CommandListまたはShadowSystemが無効です\n");
		return false;
	}
	if (!isPassActive)
	{
		DEBUG_LOG_ERROR("BeginShadowPassを呼ばずにEndShadowPassが呼ばれました\n");
		return false;
	}
	_commandList->OMSetRenderTargets(0, nullptr, FALSE, nullptr); // ShadowMapをOMから外しておく
	// 書き込みが終了したので通常描画のPSから参照できるStateへ移行する
	if (!TransitionResource(_commandList, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE)) return false;

	isPassActive = false;
	return true;
}

D3D12_GPU_VIRTUAL_ADDRESS ShadowSystem::GetFrameGPUAddress()
{
	if (!IsReady() || !hasValidLightMatrices)
	{
		DEBUG_LOG_ERROR("Shadow用行列をGPUへ転送できません\n");
		return 0;
	}
	// 同一フレームですでに転送済みなら、そのアドレスを再利用する
	if (frameGPUAddress != 0) 	return frameGPUAddress;

	ShadowFrameCB shadowData{};
	shadowData.lightViewProjection = lightViewProjectionMatrix;
	frameGPUAddress = shadowFrameRingCBV.Update(&shadowData, static_cast<UINT>(sizeof(ShadowFrameCB)));
	if (frameGPUAddress == 0) 		DEBUG_LOG_ERROR("Shadow用定数バッファのGPU転送に失敗しました\n");
	return frameGPUAddress;
}

bool ShadowSystem::CreateShadowMap(UINT _resolution)
{
	if (!device || _resolution == 0)
	{
		DEBUG_LOG_ERROR("ShadowMap作成に必要な設定が不正です\n");
		return false;
	}

	// ShadowMap本体作成
	D3D12_HEAP_PROPERTIES heapProps{};
	heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Alignment = 0;
	resourceDesc.Width = _resolution;
	resourceDesc.Height = _resolution;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.Format = DXGI_FORMAT_R32_TYPELESS; 	// 同じ領域をDSV,SRVで異なる形式で扱うので型を固定しないTYPELESSを設定
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.SampleDesc.Quality = 0;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL; // DSV
	
	// 深度は毎フレーム1.0fでクリアする ClearValueには実際にDSVで使う型つきFormatを指定する
	D3D12_CLEAR_VALUE clearValue{};
	clearValue.Format = DXGI_FORMAT_D32_FLOAT;
	clearValue.DepthStencil.Depth = 1.0f;
	clearValue.DepthStencil.Stencil = 0;

	ComPtr<ID3D12Resource> newShadowMap{};
	const HRESULT result{ device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &resourceDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&newShadowMap)) };
	if (FAILED(result))
	{
		DEBUG_LOG_ERROR("ShadowMapリソースの作成に失敗しました HRESULT : 0x{:08X}\n", static_cast<unsigned int>(result));
		return false;
	}
	newShadowMap->SetName(L"Directional Shadow Map");
	
	// Descriptorの確保
	DescriptorManager& descriptorManager{ DescriptorManager::Instance() };
	const DescriptorHandle newDSV{ descriptorManager.Allocate(HeapType::DSV) };
	if (!newDSV.IsValid())
	{
		DEBUG_LOG_ERROR("ShadowMa用DSVの確保に失敗しました\n");
		return false;
	}
	const DescriptorHandle newSRV{ descriptorManager.Allocate(HeapType::CBV_SRV_UAV) };
	if (!newSRV.IsValid())
	{
		descriptorManager.Free(HeapType::DSV, newDSV); // DSVはまだメンバに代入していないのでShutdonwでは解放されないのでここで解放
		DEBUG_LOG_ERROR("ShadowMap用SRVの作成に失敗しました\n");
		return false;
	}

	// DSVの作成
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(newShadowMap.Get(), &dsvDesc, newDSV.cpu);

	// SRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = DXGI_FORMAT_R32_FLOAT; // Shaderからは通常の32bit floatとして深度を読む
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING; // 0 = R, 1 = G, 2 = B, 3 = A をそのままマッピングするマクロ
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.PlaneSlice = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	device->CreateShaderResourceView(newShadowMap.Get(), &srvDesc, newSRV.cpu);

	// すべて成功したらメンバに確定する
	shadowMap = newShadowMap;
	shadowDSV = newDSV;
	shadowSRV = newSRV;
	resolution = _resolution;
	resourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

	return true;
}

bool ShadowSystem::TransitionResource(ID3D12GraphicsCommandList* _commandList, D3D12_RESOURCE_STATES _nextState)
{
	if (!_commandList || !shadowMap)
	{
		DEBUG_LOG_ERROR("ShadowMapのResourceStateを変更できません CommandListまたはShadowMapが向こうです\n");
		return false;
	}
	// すでに目的のStateならBarrierを発行しない
	if (resourceState == _nextState) return true;

	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = shadowMap.Get();
	barrier.Transition.StateBefore = resourceState; // CPU側で記録している現在のState
	barrier.Transition.StateAfter = _nextState; // この後の処理で必要になるState
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES; // ShadowMapには単純な1枚のテクスチャのため0でも良いが可読性を重視してALL_SUBRESOURCESを使っている
	_commandList->ResourceBarrier(1, &barrier);
	resourceState = _nextState;
	return true;
}
