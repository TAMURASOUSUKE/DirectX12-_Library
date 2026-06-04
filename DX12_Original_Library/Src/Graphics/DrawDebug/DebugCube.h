#pragma once
#include <d3d12.h>
#include "../Component/Transform.h"
#include "../Math/TSMath.h"
#include "../GraphicsType.h"

#pragma comment(lib, "d3d12.lib")

class DebugCube
{
public:
	void Initialize(); // 初期化
	void Draw(ID3D12GraphicsCommandList* _commandList); // 描画命令
	void SetRotation(Vector3 _angle); // 回転設定
	Mat4x4 GetWorldMat() const { return transform.GetWorldMatrix(); } // World行列を取得する
 
private:
	Transform transform; // Cubeの位置や回転、スケール等の情報を持ったコンポ―ネント
	VertexBuffer vertexBuffer; // ReosurceManagerが返す構造体を保持する
	IndexBuffer indexBuffer; // インデックスバッファを保持する

};