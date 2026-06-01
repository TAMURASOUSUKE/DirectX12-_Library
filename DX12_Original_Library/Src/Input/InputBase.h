#pragma once
// 入力を抽象化する際の規定となるクラス
class InputBase
{
public:
	virtual void Update() = 0; // 入力更新
	virtual bool IsPress(int _key) = 0; // 押している間
	virtual bool IsPushed(int _key) = 0; // 押した瞬間
	virtual bool IsReleased(int _key) = 0; // 話した瞬間
};