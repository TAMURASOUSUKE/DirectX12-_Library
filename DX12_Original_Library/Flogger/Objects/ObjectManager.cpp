#include "ObjectManager.h"

void ObjectManager::Register(std::unique_ptr<ObjectBase> _obj)
{
	objects.push_back(std::move(_obj));
}

void ObjectManager::Update()
{
	for (const std::unique_ptr<ObjectBase>& obj : objects)
	{
		if (!obj->GetIsActive()) continue; // 死んでいればスキップ
		obj->Update();
	}

	CheckCollision();
}

void ObjectManager::Draw()
{
	for (const std::unique_ptr<ObjectBase>& obj : objects)
	{
		if (!obj->GetIsActive()) continue; // 死んでいればスキップ
		obj->Draw();
	}
}

void ObjectManager::CheckCollision()
{
	for (size_t i = 0; i < objects.size(); ++i)
	{
		ObjectBase& obj01{ *objects[i] }; // 一つ目を取り出す
		if (!obj01.GetIsActive()) continue;
		if (obj01.GetObjectType() == ObjectType::Background) continue;

		for (size_t j = i + 1; j < objects.size(); ++j)
		{
			ObjectBase& obj02{ *objects[j] }; // 二つ目を取り出す
			if (!obj02.GetIsActive()) continue;
			if (obj02.GetObjectType() == ObjectType::Background) continue;

			// 判定を取り出す
			const Rect rect01{ obj01.GetCollisionRect() };
			const Rect rect02{ obj02.GetCollisionRect() };
			// 判定
			if (!Collision::Intersect(rect01, rect02)) continue;

			// 衝突した両方へ通知する
			obj01.OnCollision(obj02);
			obj02.OnCollision(obj01);
		}
	}
}