#pragma once
#include <vector>
#include <span>
#include <cmath>
#include "../Src/Facade/TSLib.h"

// VFX03（ワールド空間VFXメッシュ）のテスト
struct VfxMeshTest
{
	// 見る位置に合わせて調整する
	Vector3 center{ 0.0f, 42.0f, 0.0f }; // テストの中心
	float sweepRadiusRoot{ 1.0f }; // 剣の根元の半径
	float sweepRadiusTip{ 3.0f };  // 剣先の半径
	float sweepSpeed{ 3.0f };      // 1秒あたりの回転(ラジアン)
	std::size_t trailPointCount{ 32 }; // 帯に残す点の数

	ShaderHandle pixelShader{};
	MaterialHandle quadAdditive{};   // 加算・両面（カメラ正対の板）
	MaterialHandle quadBlend{};      // 半透明・片面（Y軸固定とNoneの板）
	MaterialHandle trailBlend{};     // 半透明・両面（帯）

	std::vector<Vector3> rootHistory{};  // 根元の履歴（古い→新しい）そのまま
	std::vector<Vector3> tipHistory{};   // 剣先の履歴（古い→新しい）そのまま
	std::vector<Vector3> trailLeft{};    // 描画に渡す左の点（古いほど中央へ寄せて幅を細くする）
	std::vector<Vector3> trailRight{};   // 描画に渡す右の点
	std::vector<Vector4> trailColors{};  // 点ごとの色（古いほど薄い）
	float time{ 0.0f };

	bool Setup()
	{
		pixelShader = Gfx::LoadShader(L"VfxMeshTestPS.hlsl", ShaderUsage::VFX, ShaderStage::Pixel);
		if (!pixelShader.IsValid()) return false;

		MaterialRenderDesc additiveDesc{};
		additiveDesc.blendMode = MaterialBlendMode::Additive;
		additiveDesc.doubleSided = true;
		MaterialRenderDesc blendDesc{};
		blendDesc.blendMode = MaterialBlendMode::Blend;
		blendDesc.doubleSided = false;
		MaterialRenderDesc trailDesc{};
		trailDesc.blendMode = MaterialBlendMode::Blend;
		trailDesc.doubleSided = true;

		quadAdditive = Gfx::CreateMaterial(pixelShader, additiveDesc);
		quadBlend = Gfx::CreateMaterial(pixelShader, blendDesc);
		trailBlend = Gfx::CreateMaterial(pixelShader, trailDesc);
		if (!quadAdditive.IsValid() || !quadBlend.IsValid() || !trailBlend.IsValid()) return false;

		rootHistory.reserve(trailPointCount);
		tipHistory.reserve(trailPointCount);
		trailLeft.reserve(trailPointCount);
		trailRight.reserve(trailPointCount);
		trailColors.reserve(trailPointCount);
		return true;
	}

	void Shutdown()
	{
		Gfx::Unload(trailBlend);
		Gfx::Unload(quadBlend);
		Gfx::Unload(quadAdditive);
		Gfx::Unload(pixelShader);
	}

	// 剣を中心の周りで振り続け、根元と剣先の位置を履歴に積む
	void Update(float _deltaTime)
	{
		time += _deltaTime;
		const float angle{ time * sweepSpeed };
		const Vector3 direction{ std::cos(angle), 0.0f, std::sin(angle) };
		const Vector3 root{ center + direction * sweepRadiusRoot };
		const Vector3 tip{ center + direction * sweepRadiusTip + Vector3{ 0.0f, std::sin(angle * 2.0f) * 0.5f, 0.0f } }; // 剣先は上下にも揺らす

		rootHistory.push_back(root);
		tipHistory.push_back(tip);
		if (rootHistory.size() > trailPointCount)
		{
			rootHistory.erase(rootHistory.begin());
			tipHistory.erase(tipHistory.begin());
		}

		// 描画に渡す点列を作り直す
		// 新しさ t は 一番古い点が 0、一番新しい点が 1
		// 幅: 根元と剣先の中央へ t の割合で寄せる（古いほど細く、一番古い端は線になる）
		// 色: αを t で下げる（古いほど薄い）
		trailLeft.clear();
		trailRight.clear();
		trailColors.clear();
		const std::size_t count{ rootHistory.size() };
		for (std::size_t i = 0; i < count; i++)
		{
			const float t{ count > 1 ? static_cast<float>(i) / static_cast<float>(count - 1) : 1.0f };
			const float widthScale{ t }; // 直線で細くする（急に細くしたいなら t * t）
			const Vector3 middle{ (rootHistory[i] + tipHistory[i]) * 0.5f };
			const Vector3 halfSpan{ (tipHistory[i] - rootHistory[i]) * 0.5f };
			trailLeft.push_back(middle - halfSpan * widthScale);
			trailRight.push_back(middle + halfSpan * widthScale);
			trailColors.push_back({ 1.0f, 0.9f, 0.3f, t * 0.8f });
		}
	}

	void Draw()
	{
		// 1. カメラ正対・加算。カメラをどう回しても正面を向き、重なった所が明るくなる
		Gfx::DrawQuad3D(center + Vector3{ -5.0f, 0.0f, 0.0f }, { 2.0f, 2.0f }, quadAdditive, BillboardMode::FaceCamera, Quaternion::Identity, { 0.2f, 0.8f, 1.0f, 0.8f });
		Gfx::DrawQuad3D(center + Vector3{ -4.5f, 0.3f, 0.5f }, { 2.0f, 2.0f }, quadAdditive, BillboardMode::FaceCamera, Quaternion::Identity, { 1.0f, 0.4f, 0.2f, 0.8f });

		// 2. Y軸固定・半透明。横に回り込むと正面を向くが、上から見ると板が寝ている
		Gfx::DrawQuad3D(center + Vector3{ 5.0f, 0.0f, 0.0f }, { 2.0f, 3.0f }, quadBlend, BillboardMode::AxisY, Quaternion::Identity, { 1.0f, 0.2f, 0.2f, 0.5f });

		// 3. 固定向き・半透明。回転で向きを決め、裏から見ると片面なので消える
		const Quaternion tilt{ Quaternion::FromAxisAngle(Vector3::Right, 0.5f) };
		Gfx::DrawQuad3D(center + Vector3{ 0.0f, 4.0f, 0.0f }, { 3.0f, 1.5f }, quadBlend, BillboardMode::None, tilt, { 0.2f, 1.0f, 0.2f, 0.5f });

		// 4. 帯。剣の軌跡が毎フレーム更新され、古い端が薄い
		if (trailLeft.size() >= 2)
		{
			Gfx::DrawTrail3D(trailLeft, trailRight, trailColors, trailBlend);
		}
	}
};
