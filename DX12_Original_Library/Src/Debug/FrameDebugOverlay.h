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

	// 次のChannelを表示対象にする
	void SelectNextChannel(const FrameDebugSystem& _system);

	// 前のChannelを表示対象にする
	void SelectPreviousChannel(const FrameDebugSystem& _system);

	// 構築済みの描画命令を読み取り専用で取得する
	const DebugOverlayFrame& GetFrame() const { return overlayFrame; }

	// Overlay全体の表示状態を変更する
	void SetVisible(bool _visible) { visible = _visible; }

	// 現在表示する設定か
	bool IsVisible() const { return visible; }

private:
	DebugOverlayFrame overlayFrame{};
	std::size_t selectedChannelIndex{ 0 }; // 現在選択中のチャンネルインデックス(表示非表示に使う)
	bool visible{ false }; // 表示設定を持つ

};
