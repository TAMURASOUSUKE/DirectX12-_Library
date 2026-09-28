#include <utility>
#include "../Debug/DebugLogs.h"
#include "GraphicsMetrics.h"

namespace {
	GraphicsFrameMetrics writeFrame{};
	GraphicsFrameMetrics readFrame{};
}

void GraphicsMetrics::BeginFrame()
{
	// 空でリセット
	writeFrame = GraphicsFrameMetrics{};
}

void GraphicsMetrics::EndFrame()
{
	// 読み込みと書き込みの入れ替え
	std::swap(writeFrame, readFrame);
}

void GraphicsMetrics::RecordDrawCall(GraphicsPass _pass)
{
	const std::size_t index{ static_cast<std::size_t>(_pass) };
	if (index >= writeFrame.passDrawCallCount.size())
	{
		DEBUG_LOG_ERROR("渡されたパスが配列外です\n");
		return;
	}

	writeFrame.drawCallCount++; // 全体の加算
	writeFrame.passDrawCallCount[index]++; // 指定パスのドローカウントを増やす
}

const GraphicsFrameMetrics& GraphicsMetrics::GetLastFrame()
{
	return readFrame;
}
