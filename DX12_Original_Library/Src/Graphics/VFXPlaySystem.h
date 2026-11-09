#pragma once
#include <vector>
#include "GfxType.h"
#include "GraphicsType.h"
#include "GraphicsConstant.h"
#include "../Core/Handle/VFXHandle.h"
#include "../Core/Handle/VFXParticleTypeHandle.h"
#include "../Math/Vector/Vector3.h"

class VFXMeshSystem;

// VFXの再生管理
class VFXPlaySystem
{
public:
	VFXPlaySystem() = default;
	~VFXPlaySystem() = default;

	VFXPlaySystem(const VFXPlaySystem& _other) = delete;
	VFXPlaySystem& operator=(const VFXPlaySystem& _other) = delete;

	// 描画登録先のVFXMeshSystem(非所有)を受け取る
	bool Setup(VFXMeshSystem& _meshSystem);

	// 全再生枠のBufferとMaterialを解放する
	void Shutdown();

	// 粒子の種類を登録し同時再生数ぶんの枠(粒子Buffer カウンタ Materialの複製)を作る
	VFXParticleTypeHandle RegisterParticle(const VFXParticleDesc& _desc);

	// 空き枠を1つ借りて再生開始する 空きが無ければ無効ハンドル
	VFXHandle Play(VFXParticleTypeHandle _particleType, const Vector3& _position);

	// 経過を進めてCompute要求と描画登録を行う 終了した更新で枠を返してfalse
	bool Update(VFXHandle _handle, float _deltaTime);

	// 発生位置を設定する
	bool SetPosition(VFXHandle _handle, const Vector3& _position);

	// 終了していればtrue(無効ハンドル 古いハンドルもtrue)
	bool IsFinished(VFXHandle _handle);

	// 既定は発生だけ止めて余韻を残す _immediateなら全粒子を消して枠を返す
	bool Stop(VFXHandle _handle, bool _immediate);

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

private:
	std::vector<VFXParticleTypeData> particleTypes{}; // 登録された粒子の種類の台帳
	std::vector<VFXPlaySlot> slots{}; // 再生枠の台帳 種類ごとにまとまって並ぶ
	std::vector<ComputeDispatchRequest> dispatchRequests{}; // このフレームに積んだCompute実行要求
	uint32_t frameSeed{ 0 }; // 更新ごとに進めてrandomSeedへ渡す
	uint32_t droppedPlayCount{ 0 }; // 空き無しで断った再生開始の回数
	VFXMeshSystem* meshSystem{ nullptr }; // 非所有 描画登録先
	bool isInitialized{ false }; // Setup済みか
};
