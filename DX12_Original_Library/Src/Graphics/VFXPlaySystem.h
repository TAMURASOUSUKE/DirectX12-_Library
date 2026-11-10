#pragma once
#include <vector>
#include "GfxType.h"
#include "GraphicsType.h"
#include "../Core/Handle/VFXHandle.h"
#include "../Core/Handle/VFXParticleTypeHandle.h"
#include "../Math/Vector/Vector3.h"
#include "../Core/Handle/VFXMeshTypeHandle.h"
#include "../Math/Quaternion/Quaternion.h"

class VFXMeshSystem;
class CameraSystem;

// VFXの再生管理
class VFXPlaySystem
{
public:
	VFXPlaySystem() = default;
	~VFXPlaySystem() = default;

	VFXPlaySystem(const VFXPlaySystem& _other) = delete;
	VFXPlaySystem& operator=(const VFXPlaySystem& _other) = delete;

	// 描画登録先のVFXMeshSystemと向きの計算用のカメラシステムを受け取る
	bool Setup(VFXMeshSystem& _meshSystem, const CameraSystem& _cameraSystem);

	// 全再生枠のBufferとMaterialを解放する
	void Shutdown();

	// 粒子の種類を登録し同時再生数ぶんの枠(粒子Buffer カウンタ Materialの複製)を作る
	VFXParticleTypeHandle RegisterParticle(const VFXParticleDesc& _desc);

	// メッシュの種類を登録し同時再生数ぶんの枠(Materialの複製 帯なら点の区間)を作る
	VFXMeshTypeHandle RegisterMesh(const VFXMeshDesc& _desc);

	// 空き枠を1つ借りて再生開始する 空きが無ければ無効ハンドル
	VFXHandle Play(VFXParticleTypeHandle _particleType, const Vector3& _position);

	// 空き枠を1つ借りてメッシュの再生を開始する 空きが無ければ無効ハンドル
	VFXHandle Play(VFXMeshTypeHandle _meshType, const Vector3& _position, const Quaternion& _rotation);

	// 経過を進めてCompute要求と描画登録を行う 終了した更新で枠を返してfalse
	bool Update(VFXHandle _handle, float _deltaTime);

	// 発生位置を設定する
	bool SetPosition(VFXHandle _handle, const Vector3& _position);

	// 終了していればtrue(無効ハンドル 古いハンドルもtrue)
	bool IsFinished(VFXHandle _handle);

	// 既定は発生だけ止めて余韻を残す _immediateなら全粒子を消して枠を返す
	bool Stop(VFXHandle _handle, bool _immediate);

	// 帯へこのフレームの左右の点を1組渡す 帯でない枠や受け付け終了後はfalse
	bool SetTrailPoint(VFXHandle _handle, const Vector3& _left, const Vector3& _right);

	// このフレームに積んだCompute実行要求を返す
	const std::vector<ComputeDispatchRequest>& GetDispatchRequests() const { return dispatchRequests; }

	// フレーム先頭で呼ぶ 積んだCompute実行要求を空にする
	void ClearDispatchRequests() { dispatchRequests.clear(); }

private:
	// ハンドルから再生枠の状態を引く 無効 範囲外 世代不一致ならnullptr
	VFXPlayData* Lookup(VFXHandle _handle);

	// 再生中を下ろして世代を進める
	void Release(VFXHandle _handle);

	// Buffer2本とMaterialを解放する
	void ReleaseResources(VFXPlayData& _data);

	// カウンタのリセットと更新のCompute要求を積む
	void PushDispatchRequests(const VFXPlayData& _data, const VFXParticleDesc& _desc, float _deltaTime, uint32_t _emitCount, const Vector3& _emitVelocity);

	// 板の終了判定 時間の設定 描画登録を行う 終了した更新で枠を返してfalse
	bool UpdateQuad(VFXHandle _handle, VFXPlayData& _data, const VFXMeshDesc& _desc);

	// 帯の点の寿命を進めて終了判定 時間の設定 描画登録を行う 終了した更新で枠を返してfalse
	bool UpdateTrail(VFXHandle _handle, VFXPlayData& _data, const VFXMeshDesc& _desc, float _deltaTime);

	// 経過と進み具合を枠のMaterialへ書く
	void WriteTimeParameter(MaterialHandle _material, float _elapsed, float _duration);

private:
	std::vector<VFXParticleTypeData> particleTypes{}; // 登録された粒子の種類の台帳
	std::vector<VFXMeshTypeData> meshTypes{}; // 登録されたメッシュの台帳
	std::vector<VFXTrailPoint> trailPoints{}; // 全部の帯の点 枠ごとに固定の時間を使う
	std::vector<VFXVertex> trailVertexScratch{}; // 帯の頂点を毎回確保しないための使い回し領域
	std::vector<VFXPlaySlot> slots{}; // 再生枠の台帳 種類ごとにまとまって並ぶ
	std::vector<ComputeDispatchRequest> dispatchRequests{}; // このフレームに積んだCompute実行要求
	uint32_t frameSeed{ 0 }; // 更新ごとに進めてrandomSeedへ渡す
	uint32_t droppedPlayCount{ 0 }; // 空き無しで断った再生開始の回数
	VFXMeshSystem* meshSystem{ nullptr }; // 非所有 描画登録先
	const CameraSystem* cameraSystem{ nullptr }; // 非所有 板の向きの計算に使う
	bool isInitialized{ false }; // Setup済みか
};
