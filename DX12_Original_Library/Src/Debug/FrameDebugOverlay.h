#pragma once
#include "FrameDebugSystem.h"
#include "FrameDebugOverlayType.h"

// 集約された情報を描画情報へ送るクラス
class FrameDebugOverlay
{
public:
	FrameDebugOverlay() = default;
	~FrameDebugOverlay() = default;

	FrameDebugOverlay(const FrameDebugOverlay& _other) = delete;
	FrameDebugOverlay& operator=(const  FrameDebugOverlay& _other) = delete;

	// 計測結果から描画依頼を構築
	void Build(const FrameDebugSystem& _system);

	// 構築済みの描画命令を読み取り専用で取得する
	const DebugOverlayFrame& GetFrame() const { return overlayFrame; }

private:
	DebugOverlayFrame overlayFrame{};

};
