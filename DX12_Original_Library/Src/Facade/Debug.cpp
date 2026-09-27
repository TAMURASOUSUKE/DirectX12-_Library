#include "../Debug/FrameDebugSystem.h"
#include "DebugInternal.h"
#include "Debug.h"

namespace
{
	FrameDebugSystem frameDebugSystem{};
}

bool DebugInternal::Initialize()
{
	return true;
}

void DebugInternal::BeginFrame()
{
	frameDebugSystem.BeginFrame();
}

void DebugInternal::EndFrame()
{
	frameDebugSystem.EndFrame();
}

void DebugInternal::Finish()
{

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
