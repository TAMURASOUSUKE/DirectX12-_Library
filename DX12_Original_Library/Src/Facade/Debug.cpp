#include "../Debug/FrameDebugSystem.h"
#include "../Debug/FrameDebugOverlay.h"
#include "../Debug/DebugLogs.h"
#include "DebugInternal.h"
#include "Debug.h"

namespace
{
	FrameDebugSystem frameDebugSystem{}; // データの集約
	FrameDebugOverlay frameDebugOverlay{}; // 集約されたデータの描画
}

namespace
{
#ifdef _DEBUG
	bool RunFrameDebugCoreTest()
	{
		// 実際に利用するframeDebugSystemを汚さないようテスト専用のローカルインスタンスを作る
		FrameDebugSystem testSystem{};

		const DebugChannelID channelID{ testSystem.RegisterChannel("FrameDebugCoreTest") };

		if (!channelID.IsValid()) return false;

		const DebugMetricID setID{ testSystem.RegisterMetric({"SetTest", channelID, DebugMetricUnit::None, DebugMetricAggregation::Set}) };
		const DebugMetricID addID{ testSystem.RegisterMetric({"AddTest", channelID, DebugMetricUnit::None, DebugMetricAggregation::Add}) };
		const DebugMetricID maxID{ testSystem.RegisterMetric({"MaxTest", channelID, DebugMetricUnit::None, DebugMetricAggregation::Max}) };
		if (!setID.IsValid() || !addID.IsValid() || !maxID.IsValid()) return false;

		const DebugLineCommand lineCommand
		{
			channelID,
			{ 0.0f, 0.0f, 0.0f },
			{ 1.0f, 1.0f, 1.0f },
			{ 1.0f, 0.0f, 0.0f, 1.0f }
		};

		// Y軸へ45度回転した単位Quaternion
		const Quaternion rotatedBoxRotation
		{
			0.0f,
			0.38268343f,
			0.0f,
			0.92387953f
		};

		const DebugBoxCommand boxCommand
		{
			channelID,
			{ 0.0f, 1.0f, 0.0f },
			{ 1.0f, 2.0f, 1.0f },
			rotatedBoxRotation,
			{ 0.0f, 1.0f, 0.0f, 1.0f }
		};

		const DebugSphereCommand sphereCommand
		{
			channelID,
			{ 2.0f, 1.0f, 0.0f },
			1.0f,
			{ 0.0f, 0.0f, 1.0f, 1.0f }
		};

		const DebugCapsuleCommand capsuleCommand
		{
			channelID,
			{ -2.0f, 0.5f, 0.0f },
			{ -2.0f, 2.5f, 0.0f },
			0.5f,
			{ 1.0f, 1.0f, 0.0f, 1.0f }
		};

		// 1フレーム目
		testSystem.BeginFrame();

		testSystem.SubmitMetric(setID, 10.0);
		testSystem.SubmitMetric(setID, 20.0);

		testSystem.SubmitMetric(addID, 2.0);
		testSystem.SubmitMetric(addID, 3.0);
		testSystem.SubmitMetric(addID, 4.0);

		testSystem.SubmitMetric(maxID, -5.0);
		testSystem.SubmitMetric(maxID, -2.0);
		testSystem.SubmitMetric(maxID, -8.0);

		testSystem.SubmitLine(lineCommand);
		testSystem.SubmitBox(boxCommand);
		testSystem.SubmitSphere(sphereCommand);
		testSystem.SubmitCapsule(capsuleCommand);

		testSystem.EndFrame();

		const DebugFrameData& firstFrame{ testSystem.GetReadFrame() };
		const auto& firstMetrics{ firstFrame.metrics };

		// サイズや値のチェック
		if (firstMetrics.size() != 3) return false;

		if (!firstMetrics[0].written || firstMetrics[0].value != 20.0) return false;
		if (!firstMetrics[1].written || firstMetrics[1].value != 9.0) return false;
		if (!firstMetrics[2].written || firstMetrics[2].value != -2.0) return false;

		// 登録した4種類のデバッグ形状が1個ずつ存在することを確認する
		if (firstFrame.lineCommands.size() != 1) return false;
		if (firstFrame.boxCommands.size() != 1) return false;
		if (firstFrame.sphereCommands.size() != 1) return false;
		if (firstFrame.capsuleCommands.size() != 1) return false;

		// 2フレーム目
		// 前フレームの値が残っていないことを確認する
		testSystem.BeginFrame();

		testSystem.SubmitMetric(addID, 1.0);

		testSystem.EndFrame();

		const DebugFrameData& secondFrame{ testSystem.GetReadFrame() };
		const auto& secondMetrics{ secondFrame.metrics };

		// 値のチェック
		if (secondMetrics.size() != 3) return false;

		if (secondMetrics[0].written) return false;
		if (!secondMetrics[1].written || secondMetrics[1].value != 1.0) return false;
		if (secondMetrics[2].written) return false;

		// 前フレームの形状が残っていないか確認
		if (!secondFrame.lineCommands.empty()) return false;
		if (!secondFrame.boxCommands.empty()) return false;
		if (!secondFrame.sphereCommands.empty()) return false;
		if (!secondFrame.capsuleCommands.empty()) return false;

		// チャンネル無効の確認
		if (!testSystem.SetChannelEnabled(channelID, false)) return false;

		testSystem.BeginFrame();

		testSystem.SubmitLine(lineCommand);
		testSystem.SubmitBox(boxCommand);
		testSystem.SubmitSphere(sphereCommand);
		testSystem.SubmitCapsule(capsuleCommand);

		testSystem.EndFrame();

		const DebugFrameData& disabledFrame{ testSystem.GetReadFrame() };

		if (!disabledFrame.lineCommands.empty()) return false;
		if (!disabledFrame.boxCommands.empty()) return false;
		if (!disabledFrame.sphereCommands.empty()) return false;
		if (!disabledFrame.capsuleCommands.empty()) return false;

		return true;
	}


#endif
}

bool DebugInternal::Initialize()
{
#ifdef _DEBUG
	DEBUG_ASSERT(RunFrameDebugCoreTest() && "FrameDebugSystemの収集テストに失敗しました");
#endif
	return true;
}

void DebugInternal::BeginFrame()
{
	frameDebugSystem.BeginFrame();
}

void DebugInternal::EndFrame()
{
	frameDebugSystem.EndFrame(); // データの集約
	frameDebugOverlay.Build(frameDebugSystem); // 描画
}

void DebugInternal::Finish()
{

}

const DebugOverlayFrame& DebugInternal::GetOverlayFrame()
{
	return frameDebugOverlay.GetFrame();
}

const DebugFrameData& DebugInternal::GetFrameData()
{
	return frameDebugSystem.GetReadFrame();
}

DebugChannelID Debug::RegisterChannel(const std::string& _name)
{
	return frameDebugSystem.RegisterChannel(_name);
}

DebugMetricID Debug::RegisterMetric(const DebugMetricDescriptor& _descriptor)
{
	return frameDebugSystem.RegisterMetric(_descriptor);
}

void Debug::SubmitMetric(DebugMetricID _metricID, double _value)
{
	frameDebugSystem.SubmitMetric(_metricID, _value);
}

void Debug::SubmitLine(const DebugLineCommand& _command)
{
	frameDebugSystem.SubmitLine(_command);
}

void Debug::SubmitBox(const DebugBoxCommand& _command)
{
	frameDebugSystem.SubmitBox(_command);
}

void Debug::SubmitSphere(const DebugSphereCommand& _command)
{
	frameDebugSystem.SubmitSphere(_command);
}

void Debug::SubmitCapsule(const DebugCapsuleCommand& _command)
{
	frameDebugSystem.SubmitCapsule(_command);
}

void Debug::SetOverlayVisible(bool _visible)
{
	frameDebugOverlay.SetVisible(_visible);
}

bool Debug::IsOverlayVisible()
{
	return frameDebugOverlay.IsVisible();
}

bool Debug::SetChannelEnabled(DebugChannelID _channelID, bool _enabled)
{
	return frameDebugSystem.SetChannelEnabled(_channelID, _enabled);
}
