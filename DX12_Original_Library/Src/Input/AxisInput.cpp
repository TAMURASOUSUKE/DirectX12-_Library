#include <utility>
#include <variant>
#include <cstddef>
#include "GamePadInput.h"
#include "KeyboardInput.h"
#include "MouseInput.h"
#include "../Debug/DebugLogs.h"
#include "AxisInput.h"

namespace
{
	// DigitalBinding用Visitor
	struct DigitalSourceVisitor
	{
		// 物理状態を持ってくる
		KeyboardInput& kb;
		MouseInput& ms;
		GamePadInput& pad;

		bool operator()(KeyCode::Button _key) const { return kb.IsPress(static_cast<int>(_key)); }
		bool operator()(MouseCode::Click _click) const { return ms.IsPress(static_cast<int>(_click)); }
		bool operator()(PadCode::Button _button) const { return pad.IsPress(static_cast<int>(_button)); }
	};
	// 各Bindingを出すためのvisitor
	struct AxisBindingVisitor
	{
		// 物理状態を持ってくる
		KeyboardInput& kb;
		MouseInput& ms;
		GamePadInput& pad;

		AxisMode mode{AxisMode::Value};
		float unscaledDeltaTime{0.0f};

		// 各Bindingが選択されたときの挙動を整理する
		Vector2 operator()(const DigitalAxisBinding& _binding) const
		{
			// DigitalAxisBindingの中にvariantがあるのでさらにvisitで取り出す必要がある
			DigitalSourceVisitor visitor{ kb, ms, pad };
			bool isPress{ false }; // 結果判定
			isPress = std::visit(visitor, _binding.source); // DigitalAxisBindingの中のvariantから取り出す
			if (isPress)
			{
				// いずれかの設定値が押されている場合
				float actualDeltaTime{ mode == AxisMode::Delta ? unscaledDeltaTime : 1.0f }; // モードがDeltaTimeなら値が入る
				return _binding.direction * _binding.scale * actualDeltaTime;
			}
			else
			{
				// 押されていない時はゼロを返す
				return Vector2::Zero;
			}
		}
		Vector2 operator()(const StickAxisBinding& _binding) const
		{
			float actualDeltaTime{ mode == AxisMode::Delta ? unscaledDeltaTime : 1.0f }; // モードがDeltaTimeなら値が入る
			return pad.GetStickValue(_binding.stick, _binding.isYInverted) * _binding.scale * actualDeltaTime;
		}
		Vector2 operator()(const TriggerAxisBinding& _binding) const
		{
			float actualDeltaTime{ mode == AxisMode::Delta ? unscaledDeltaTime : 1.0f }; // モードがDeltaTimeなら値が入る
			return _binding.direction *  pad.GetTriggerValue(_binding.trigger) * _binding.scale * actualDeltaTime;
		}
		Vector2 operator()(const MouseDeltaAxisBinding& _binding) const
		{
			// MouseのDeltaは１ピクセルあたりの変化量 すでに1フレーム分なのでDeltaTimeを掛けない
			// マウス以外のDeltaは1秒あたりの変化量
			Vector2Int actualCursorDelta{ ms.GetCursorDelta()};
			return Vector2{ static_cast<float>(actualCursorDelta.x), _binding.isYInverted ? static_cast<float>(-actualCursorDelta.y) : static_cast<float>(actualCursorDelta.y) } * _binding.scale;
		}
	};
}

void AxisInput::SetupAxisCount(int  _count)
{
	// 0以上が渡される前提なのでDebugの場合違反していたら止める
	DEBUG_ASSERT((_count > 0) && "Axis抽象化入力初期化に0値が渡されています\n");
	if (_count <= 0)
	{
		// 0以下ならクリアする
		axes.clear();
		return;
	}
	axes.clear();
	axes.resize(static_cast<std::size_t>( _count));
}

void AxisInput::SetAxisMode(int  _axis, AxisMode _mode)
{
	std::size_t index{ static_cast<std::size_t>(_axis) };
	if (!IsInSizeLimit(index))
	{
		DEBUG_LOG_ERROR("SetAxisModeにて不正なaxisが渡されました axis : {} axisCount : {}\n", _axis, axes.size());
		return;
	}
	axes[index].calculationMode = _mode;
}

bool AxisInput::AddAxisBinding(int _axis, AxisBinding _binding)
{
	std::size_t index{ static_cast<std::size_t>(_axis) };
	if (!IsInSizeLimit(index))
	{
		DEBUG_LOG_ERROR("AddAxisBindingにて不正なaxisが渡されました axis : {} axisCount : {}\n", _axis, axes.size());
		return false;
	}
	axes[index].bindings.push_back(std::move(_binding));
	return true;
}

void AxisInput::Update(KeyboardInput& _keyboard, MouseInput& _mouse, GamePadInput& _pad, float _unscaledDeltaTime)
{
	for (auto& axis : axes)
	{
		axis.calculatedValue = Vector2::Zero; // 最初に戻す
		AxisBindingVisitor visitor{ _keyboard, _mouse, _pad, axis.calculationMode, _unscaledDeltaTime }; // 各状態をvisitorに渡す
		for (const auto& axisBinding : axis.bindings)
		{
			axis.calculatedValue +=  std::visit(visitor, axisBinding); // 寄与値を加算
		}
		// Valueかつ長さが1を超えているなら正規化する
		if (axis.calculationMode == AxisMode::Value && axis.calculatedValue.LengthSquared() > 1.0f) axis.calculatedValue.Normalize();
	}
}

Vector2 AxisInput::GetValue(int  _axis) const
{
	std::size_t index{ static_cast<std::size_t>(_axis) };
	if (!IsInSizeLimit(index))
	{
		DEBUG_LOG_ERROR("GetValueにて不正なaxisが渡されました axis : {} axisCount : {}\n", _axis, axes.size());
		return Vector2::Zero;
	}
	return axes[index].calculatedValue;
}

bool AxisInput::IsInSizeLimit(std::size_t _axis) const
{
	if (_axis >= axes.size()) return false;
	return true;
}
