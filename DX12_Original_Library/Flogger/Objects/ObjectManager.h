#pragma once
#include <vector>
#include <memory>
#include "../../Src/Facade/TSLib.h"
#include "ObjectBase.h"

// オブジェクト管理
class ObjectManager
{
public:
	static ObjectManager& Instance()
	{
		static ObjectManager instance;
		return instance;
	}

	void Register(std::unique_ptr<ObjectBase> _obj); // 配列へ登録
	void Update();
	void Draw();
private:
	ObjectManager() = default;

	void CheckCollision(); // 衝突探索

private:
	std::vector<std::unique_ptr<ObjectBase>> objects{};
	int playerLife{ 0 };
};