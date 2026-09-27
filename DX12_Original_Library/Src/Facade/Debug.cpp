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

		testSystem.EndFrame();

		const auto& firstFrame{ testSystem.GetReadFrame().metrics };

		// サイズや値のチェック
		if (firstFrame.size() != 3) return false;

		if (!firstFrame[0].written || firstFrame[0].value != 20.0) return false;
		if (!firstFrame[1].written || firstFrame[1].value != 9.0) return false;
		if (!firstFrame[2].written || firstFrame[2].value != -2.0) return false;

		// 2フレーム目
		// 前フレームの値が残っていないことを確認する
		testSystem.BeginFrame();

		testSystem.SubmitMetric(addID, 1.0);

		testSystem.EndFrame();

		const auto& secondFrame{ testSystem.GetReadFrame().metrics };

		// 値のチェック
		if (secondFrame.size() != 3) return false;

		if (secondFrame[0].written) return false;
		if (!secondFrame[1].written || secondFrame[1].value != 1.0) return false;
		if (secondFrame[2].written) return false;

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
