#pragma once
#include <memory>
#include <type_traits>
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ModelHandle.h"
#include "../Core/Handle/ShaderHandle.h"
#include "../Core/Handle/MaterialHandle.h"
#include "../Core/Handle/RTHandle.h"
#include "../Graphics/GraphicsType.h" // アニメーションのテスト用に持ってきているが本来見せない
#include "../Component/Transform.h"
#include "../Math/TSMath.h"

// グラフィックスに関する機能をユーザーに簡易的に提供するためのファイル
namespace Gfx 
{

	// ユーザーが触ってはならない空間を名前で知らせる
	namespace Detail
	{
		// テンプレートから呼び出される内部実装
		bool SetMaterialParameterRaw(MaterialHandle _handle, size_t _slot , const void* _data, size_t _dataSize);
	}

	// フォントアトラスの設定構造体(デフォルトでフォントを用意しているが変更したい時にここを設定してもらう)
	struct BitmapFont
	{
		TexHandle texture; // アトラス画像のハンドル
		int texWidth{ 0 }; // テクスチャ全体の幅(px)
		int texHeight{ 0 }; // テクスチャ全体の高さ(px)
		int cellWidth{ 0 }; // 1セルの幅(px)
		int cellHeight{ 0 }; // 1セルの高さ(px)
		int cols{ 0 }; // 1行あたりのセルの量
		int firstCode{ 0 }; // 先頭セルが表す文字コード(CP437配列なら0, スペース始まりなら32)
	};

	// 等間隔のグリッド状に分割されたテクスチャアトラス
	struct TextureAtlas
	{
		TexHandle texture{}; // アトラス画像本体
		int columns{ 0 };    // 横方向の分割数
		int rows{ 0 };       // 縦方向の分割数
		int frameCount{ 0 }; // 実際に使用するセル数

		bool IsValid() const 
		{
			const long long capacity{ static_cast<long long>(columns) * rows };
			return texture.IsValid() && columns > 0 && rows > 0 && frameCount > 0 && frameCount <= capacity;
		}

	};


	// 画像をどの用途として読み込むか
	enum class TextureUsage
	{
		Color, // 表示用カラー画像(sRGBとして読み込む) 
		Data, // ハイトマップやノーマルマップなどの数値データ,Linerとして読み込む	
	};


	// 画面のクリア(引数で色を設定できるデフォルトは黒)
	void ClearScreen(float _r = 0.0f, float _g = 0.0f, float _b = 0.0f, float _a = 1.0f);


	//画像読み込み : ファイル名とどの用途として読み込むか(ノーマルマップなどの数値データならColorではなくDataとしてください)
	TexHandle LoadTexture(const char* _filePath, TextureUsage _usage = TextureUsage::Color);
	// アトラス画像読み込み
	TextureAtlas LoadTextureAtlas(const char* _filePath, int _columns, int _rows, int _frameCount = 0);
	// モデル読み込み
	ModelHandle LoadModel(const char* _filePath);
	// Shader読み込み
	ShaderHandle LoadShader(const wchar_t* _filePath, ShaderUsage _usage, ShaderStage _stage);
	// material読み込み(内蔵VSを使う簡易版)
	MaterialHandle CreateMaterial(ShaderHandle _pixel);
	// material読み込み(VSも指定する版)
	MaterialHandle CreateMaterial(ShaderHandle _vertexShader, ShaderHandle _pixelShader);


	// 矩形描画
	void DrawBox(Vector2 _leftTop, Vector2 _rightBottom, float _radRotation = 0.0f, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }, bool _isWireframe = false);
	// 円描画
	void DrawCircle(Vector2 _center, float _radius, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }, bool _isWireframe = false);
	// カプセル描画
	void DrawCapsule(Vector2 _startPos, Vector2 _endPos, float _radius, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f }, bool _isWireframe = false);
	// 線分描画
	void DrawLine(Vector2 _startPos, Vector2 _endPos, Vector4 _color = { 1.0f, 1.0f, 1.0f ,1.0f });


	// 文字列描画 ; デフォルトフォント使用版(文字列, 位置, スケール(デフォルト1.0f), 描画レイヤー(デフォルト前面))
	void DrawString(const char* _string, Vector2 _position, float _scale = 1.0f, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// 文字描画 : 独自フォント使用版(フォント(構造体による別途設定必須), 文字列, 位置, スケール(デフォルト1.0f), 描画レイヤー(デフォルト前面))
	void DrawString(const BitmapFont& _font, const char* _string, Vector2 _position, float _scale = 1.0f,Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	
	
	//  スプライト描画(位置、倍率、画像, 回転角度(ラジアンかつデフォルトは0),色, uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _scale = Vector2::One, float _radRotation = 0.0f, Vector4 _color = Vector4::One, Vector2 _uvMin = Vector2::Zero, Vector2 _uvMax = Vector2::One, RenderLayer _layer = RenderLayer::ForeGround);
	// アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, 倍率, 回転, 色, uv最小値, uv最大値, レイヤー)
	void DrawSprite(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _scale = Vector2::One, float _radRotation = 0.0f, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// スプライト描画(位置、ピクセル幅、画像, 回転角度(ラジアンかつデフォルトは0),色, uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSpriteSized(TexHandle _texture, Vector2 _position, Vector2 _pixelSize, float _radRotation = 0.0f, Vector4 _color = Vector4::One, Vector2 _uvMin = { Vector2::Zero }, Vector2 _uvMax = { Vector2::One }, RenderLayer _layer = RenderLayer::ForeGround);
	// アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, ピクセル幅, 回転, 色, uv最小値, uv最大値, レイヤー)
	void DrawSpriteSized(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _pixelSize, float _radRotation = 0.0f, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用スプライト描画(位置、倍率、画像, material, 回転角度(ラジアンかつデフォルトは0), 色,  uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSprite(TexHandle _texture, Vector2 _position, MaterialHandle _material , Vector2 _scale = Vector2::One, float _radRotation = 0.0f, Vector4 _color = Vector4::One, Vector2 _uvMin = Vector2::Zero, Vector2 _uvMax = Vector2::One, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, material,倍率, 回転, 色, uv最小値, uv最大値, レイヤー)
	void DrawSprite(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, MaterialHandle _material,Vector2 _scale = Vector2::One, float _radRotation = 0.0f, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用スプライト描画(位置、ピクセル幅、画像, material, 回転角度(ラジアンかつデフォルトは0), 色,  uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSpriteSized(TexHandle _texture, Vector2 _position, Vector2 _pixelSize, MaterialHandle _material, float _radRotation = 0.0f, Vector4 _color = Vector4::One, Vector2 _uvMin = { Vector2::Zero }, Vector2 _uvMax = { Vector2::One }, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, ピクセル幅, material,回転, 色, uv最小値, uv最大値, レイヤー)
	void DrawSpriteSized(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _pixelSize, MaterialHandle _material,float _radRotation = 0.0f, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	
	
	// モデルを描画する(テスト用にAnimDataを受け取っているが後で修正)
	void DrawModel(ModelHandle _model, Transform _transform, AnimInstanceData* _animData = nullptr);


	// terrainを描画する : 位置, 大きさ(xz平面にのみかかります), 分割係数 , 高さ ,変形形状を決めるheightMap(無ければplane描画になります)
	void DrawTerrain(Vector3 _position, float _scale, float _tessFactor, float _heightScale, Vector4 _color, TexHandle _heightMap = {});
	
	
	// 色の変更(今後は引数を変更)
	void SetBaseColor(ModelHandle _model, int _submeshIndex, Vector4 _color);
	// テクスチャの変更(今後は引数を変更) : セットしたモデルがUnloadされた場合セットしたTextureは解放されません(個別で解放が必要)
	void SetTexture(ModelHandle _model, int _submeshIndex, TexHandle _texture);
	// 画面全体へ適用するポストエフェクトmaterialを設定する(無効ハンドルを渡した場合は内蔵の素通し描画へ戻します)
	void SetPostEffect(MaterialHandle _material);
	
	
	// テクスチャリソースの解放
	void Unload(TexHandle _handle);
	// モデルリソースの開放
	void Unload(ModelHandle _hanlde);
	// RenderTargetの解放
	void Unload(RTHandle _handle);
	// Shaderリソースの開放
	void Unload(ShaderHandle _handle);
	// Materialリソースの解放
	void Unload(MaterialHandle _handle);
	
	
	/// <summary>
	/// 任意のマテリアルに対して任意のslotにパラメータを設定する
	/// パラメータは最大4つまで設定できます
	/// レジスタの4-7まで置かれるので0を指定したらHLSL側ではb4, 1ならb5...となりb7まで使えます
	/// </summary>
	/// <typeparam name="T">ユーザーが作成したパラメータとなるテンプレート</typeparam>
	/// <param name="_handle">設定したいmaterial</param>
	/// <param name="_slot">設定したいslot番号</param>
	/// <param name="_parameter">定数バッファとして渡るパラメータ。任意の構造体を作り16byte区切りでパラメータを設定して下さい。HLSL側と作成したパラメータの並び順をそろえてください。ポインタやvector,string等は渡さないでください</param>
	/// <returns>設定が成功したかどうか</returns>
	template<typename T>
	bool SetMaterialParameter(MaterialHandle _handle, size_t _slot, const T& _parameter)
	{
		using ParameterType = std::remove_cv_t<std::remove_reference_t<T>>; // constや参照をはがして純粋な型を取り出す
		// vector,stringなどmemcpyだけでは複製できない型を禁止する
		static_assert(std::is_trivially_copyable_v<ParameterType>, "MaterialParameterには単純コピー可能な型を使用してください");
		// 不規則なオブジェクトレイアウトを避ける
		static_assert(std::is_standard_layout_v<ParameterType>, "MaterialParamterには標準レイアウト型を渡してください\n");
		// float*などのポインタそのものを渡す誤用を防ぐ
		static_assert(!std::is_pointer_v<ParameterType>, "MaterialParameterにポインタは使用できません\n");
		// RingConstantBufferの1スライスに収まるかコンパイル時に確認する
		static_assert(sizeof(ParameterType) <= MAX_MATERIAL_PARAMETER_SIZE, "MaterialParameterサイズが上限を超えています\n");
		return Detail::SetMaterialParameterRaw(_handle, _slot,static_cast<const void*>(std::addressof(_parameter)), sizeof(ParameterType));
	}
	/// <summary>
	/// 任意のマテリアルに対してパラメータを設定する
	/// スロットを指定していないので0として扱い、b4に配置されます
	/// </summary>
	/// <typeparam name="T">ユーザーが作成したパラメータとなるテンプレート</typeparam>
	/// <param name="_handle">設定したいmaterial</param>
	/// <param name="_parameter">定数バッファとして渡るパラメータ。任意の構造体を作り16byte区切りでパラメータを設定して下さい。HLSL側と作成したパラメータの並び順をそろえてください。ポインタやvector,string等は渡さないでください</param>
	/// <returns>設定が成功したかどうか</returns>
	template<typename T>
	bool SetMaterialParameter(MaterialHandle _handle, const T& _parameter)
	{
		return SetMaterialParameter(_handle, 0, _parameter);
	}
}
