#pragma once
#include <memory>
#include <type_traits>
#include <span>
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ModelHandle.h"
#include "../Core/Handle/ShaderHandle.h"
#include "../Core/Handle/MaterialHandle.h"
#include "../Core/Handle/AnimInstanceHandle.h"
#include "../Graphics/GfxType.h"
#include "../Component/Transform.h"
#include "../Component/Camera.h"
#include "../Component/Light.h"
#include "../Math/TSMath.h"

struct AABB; // 描画側が判定側に依存しない

// グラフィックスに関する機能をユーザーに簡易的に提供するためのファイル
namespace Gfx 
{

	// ユーザーが触ってはならない空間を名前で知らせる
	namespace Detail
	{
		// テンプレートから呼び出される内部実装
		bool SetMaterialParameterRaw(MaterialHandle _handle, std::size_t _slot , const void* _data, size_t _dataSize);
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

	// 画像の状態
	enum class SpriteFlip
	{
		None,
		Horizontal,
		Vertical,
		Both
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

	// スプライトアニメーションの設定と再生状態
	struct SpriteAnimationState
	{
		int firstFrame{ 0 };   // 使用する最初のセル
		int lastFrame{ 0 };    // 使用する最後のセル（この番号を含む）
		int currentFrame{ 0 }; // 現在表示しているセル

		float secondsPerFrame{ 0.1f }; // 1コマの表示秒数
		float elapsedTime{ 0.0f };     // コマ送り用の蓄積時間

		bool isLoop{ true }; // ループするか
		bool isFinished{ false }; // アニメーションが終了したか

		SpriteAnimationState() = default;

		// 必要な設定を引数だけで指定する
		SpriteAnimationState(int _firstFrame, int _lastFrame, float _secondsPerFrame, bool _isLoop = true) :
			firstFrame{ _firstFrame }, lastFrame{ _lastFrame }, currentFrame{ _firstFrame }, secondsPerFrame{ _secondsPerFrame }, isLoop{ _isLoop } {}

		bool IsValid(const TextureAtlas& _atlas) const
		{
			return _atlas.IsValid()
				&& firstFrame >= 0
				&& lastFrame >= firstFrame
				&& lastFrame < _atlas.frameCount
				&& currentFrame >= firstFrame
				&& currentFrame <= lastFrame
				&& secondsPerFrame > 0.0f;
		}

		int GetFrameIndex() const { return currentFrame; }

		void Reset()
		{
			currentFrame = firstFrame;
			elapsedTime = 0.0f;
			isFinished = false;
		}
	};

	// 画像をどの用途として読み込むか
	enum class TextureUsage
	{
		Color, // 表示用カラー画像(sRGBとして読み込む) 
		Data, // ハイトマップやノーマルマップなどの数値データ,Linerとして読み込む	
	};

	// 3Dの基本図形の描画方法を設定する
	enum class Primitive3DStyle
	{
		Fill, // 塗りつぶし
		MeshWireframe, // メッシュをワイヤー表示
		DebugLine, // あたり判定可視化などデバッグ途用として余計な線を描画しないワイヤー表示
	};

	// 2D描画で使用している仮想解像度を取得する
	Vector2Int GetVirtualSize();

	// 現在使用する3Dカメラを設定する(呼ぶ場合は必ず3D描画前に呼んでください)
	bool SetCamera(const Camera& _camera);

	// シーン全体で使用するライトを設定する
	bool SetSceneLight(const SceneLight& _sceneLight);

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
	// モデルをもとにアニメーション個体の作成
	AnimInstanceHandle CreateAnimInstance(ModelHandle _handle);

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
	
	
	//  スプライト描画(位置、倍率、画像, 回転角度(ラジアンかつデフォルトは0), 画像の状態,色, uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSprite(TexHandle _texture, Vector2 _position, Vector2 _scale = Vector2::One, float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, Vector2 _uvMin = Vector2::Zero, Vector2 _uvMax = Vector2::One, RenderLayer _layer = RenderLayer::ForeGround);
	// アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, 倍率, 回転, , 画像の状態,色, uv最小値, uv最大値, レイヤー)
	void DrawSprite(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _scale = Vector2::One, float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// スプライト描画(位置、ピクセル幅、画像, 回転角度(ラジアンかつデフォルトは0), 画像の状態, 色, uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSpriteSized(TexHandle _texture, Vector2 _position, Vector2 _pixelSize, float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, Vector2 _uvMin = { Vector2::Zero }, Vector2 _uvMax = { Vector2::One }, RenderLayer _layer = RenderLayer::ForeGround);
	// アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, ピクセル幅, 回転, 画像の状態,色, uv最小値, uv最大値, レイヤー)
	void DrawSpriteSized(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _pixelSize, float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用スプライト描画(位置、倍率、画像, material, 回転角度(ラジアンかつデフォルトは0), 画像の状態,色,  uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSprite(TexHandle _texture, Vector2 _position, MaterialHandle _material , Vector2 _scale = Vector2::One, float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, Vector2 _uvMin = Vector2::Zero, Vector2 _uvMax = Vector2::One, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, material,倍率, 回転, 画像の状態, 色, uv最小値, uv最大値, レイヤー)
	void DrawSprite(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, MaterialHandle _material,Vector2 _scale = Vector2::One, float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用スプライト描画(位置、ピクセル幅、画像, material, 回転角度(ラジアンかつデフォルトは0), 画像の状態, 色,  uv座標(デフォルトは左上0右下1) 描画するレイヤー(デフォルトは通常 = 3Dより手前))
	void DrawSpriteSized(TexHandle _texture, Vector2 _position, Vector2 _pixelSize, MaterialHandle _material, float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, Vector2 _uvMin = { Vector2::Zero }, Vector2 _uvMax = { Vector2::One }, RenderLayer _layer = RenderLayer::ForeGround);
	// Shader適用アトラスを用いてスプライト描画をする(指定アトラス, セルのIndex, 位置, ピクセル幅, material,回転, 画像の状態, 色, uv最小値, uv最大値, レイヤー)
	void DrawSpriteSized(const TextureAtlas& _atlas, int _frameIndex, Vector2 _position, Vector2 _pixelSize, MaterialHandle _material,float _radRotation = 0.0f, SpriteFlip _flip = SpriteFlip::None, Vector4 _color = Vector4::One, RenderLayer _layer = RenderLayer::ForeGround);
	// 設定に従って現在フレームを進める(対象アトラス, 設定, 時間)
	bool UpdateSpriteAnim(const TextureAtlas& _atlas, SpriteAnimationState& _state, float _deltaTime);

	// 単位CubeをTransformで配置して描画する
	void DrawCube3D(const Transform& _transform, Vector4 _color = {1.0f, 1.0f, 1.0f, 1.0f}, Primitive3DStyle _style = Primitive3DStyle::Fill);
	// 単位SphereをTransformで配置して描画する
	void DrawSphere3D(const Transform& _transform, Vector4 _color = {1.0f, 1.0f, 1.0f, 1.0f}, Primitive3DStyle _style = Primitive3DStyle::Fill);
	// 単位CylinderをTransformで配置して描画する
	void DrawCylinder3D(const Transform& _transform, Vector4 _color = { 1.0f, 1.0f, 1.0f, 1.0f }, Primitive3DStyle _style = Primitive3DStyle::Fill);
	// 単位Capsuleを線分と半径で配置して描画する
	void DrawCapsule3D(Vector3 _start, Vector3 _end, float _radius, Vector4 _color = { 1.0f, 1.0f, 1.0f, 1.0f }, Primitive3DStyle _style = Primitive3DStyle::Fill);
	// 単位PlaneをTransformで配置して描画する
	void DrawPlane3D(const Transform& _transform, Vector4 _color = { 1.0f, 1.0f, 1.0f, 1.0f }, Primitive3DStyle _style = Primitive3DStyle::Fill);
	// 単位Lineを始点と終点で配置する
	void DrawLine3D(Vector3 _start, Vector3 _end, Vector4 _color = {1.0f, 1.0f, 1.0f, 1.0f});
	// 平面上に3DGridを描画する(中心座標、中心から片側端までのCell数、Cell1つ分のサイズ(デフォルトは1m), 色)
	void DrawGrid3D(Vector3 _center, Quaternion _rotation = Quaternion::Identity,unsigned int _halfCellCount = 10, float _cellSize = 1.0f, Vector4 _color = { 1.0f, 1.0f, 1.0f, 1.0f });
	// ワールド空間の座標軸に沿ったグリッドを生成する(中心座標、中心から片側端までのCell数、Cell1つ分のサイズ(デフォルトは1m), 各軸のグリッドの色(デフォルトはX = 赤 Y = 緑 Z = 青です))
	void DrawWorldAxisGrid3D(Vector3 _center, unsigned int _halfCellCount = 10, float _cellSize = 1.0f, Vector4 _xAxisColor = { 1.0f, 0.0f, 0.0f, 1.0f }, Vector4 _yAxisColor = { 0.0f, 1.0f, 0.0f, 1.0f }, Vector4 _zAxisColor = { 0.0f, 0.0f, 1.0f, 1.0f });
	// ローカル座標軸を描画する
	void DrawAxis3D(Vector3 _origin, Quaternion _rotation = Quaternion::Identity, float _length = 1.0f, Vector4 _xColor = { 1.0f, 0.0f, 0.0f, 1.0f }, Vector4 _yColor = { 0.0f, 1.0f, 0.0f, 1.0f }, Vector4 _zColor = { 0.0f, 0.0f, 1.0f, 1.0f });
	// AABBをデバッグ表示する
	void DrawAABB3D(const AABB& _aabb, Vector4 _color = { 0.0f, 1.0f, 0.0f, 1.0f });


	// 静的モデルを描画する
	void DrawModel(ModelHandle _model, Transform _transform);
	// アニメーションつきモデルを描画する
	void DrawAnimatedModel(AnimInstanceHandle _handle, Transform _transform);
	// 指定したアニメーション個体のアニメーションを更新する
	bool UpdateAnim(AnimInstanceHandle _handle, float _deltaTime);
	// 指定したアニメーションを最初から最後まで再生する
	bool PlayAnim(AnimInstanceHandle _handle, int _clipIndex, bool _isLoop, float _playbackSpeed = 1.0f);
	// 指定したアニメーションを指定した範囲で再生する
	bool PlayRangeAnim(AnimInstanceHandle _handle, int _clipIndex, float _startTime, float _endTime, bool _isLoop, float _playbackSpeed = 1.0f);
	// アニメーションを別の指定アニメーションへと滑らかに遷移する
	bool CrossFadeAnim(AnimInstanceHandle _handle, int _clipIndex, float _duration, bool _isLoop, float _playbackSpeed = 1.0f);
	// ポーズされているアニメーションを再開する
	bool ResumeAnim(AnimInstanceHandle _handle);
	// 指定したアニメーションの再生を停止する(次回再生時には最初からになります)
	bool StopAnim(AnimInstanceHandle _handle);
	// 選択したアニメーションの再生を停止して維持する(再開はResumeAnimから行ってください)
	bool PauseAnim(AnimInstanceHandle _handle);
	// 指定したアニメーションが再生終了しているか(停止中は終了ではないのでfalseになります)
	bool IsFinishedAnim(AnimInstanceHandle _handle);
	// 指定したアニメーションの再生速度を変更する(負数なら逆再生になります)
	bool SetAnimPlaybackSpeed(AnimInstanceHandle _handle, float _playbackSpeed);
	// 指定したアニメーションを破棄する
	bool DestroyAnim(AnimInstanceHandle _handle);

	// terrainを描画する : 位置, 大きさ(xz平面にのみかかります), 分割係数 , 高さ ,変形形状を決めるheightMap(無ければplane描画になります)
	void DrawTerrain(Vector3 _position, float _scale, float _tessFactor, float _heightScale, Vector4 _color, TexHandle _heightMap = {});
	
	
	// 色の変更(今後は引数を変更)
	void SetBaseColor(ModelHandle _model, int _submeshIndex, Vector4 _color);
	// テクスチャの変更(今後は引数を変更) : セットしたモデルがUnloadされた場合セットしたTextureは解放されません(個別で解放が必要)
	void SetTexture(ModelHandle _model, int _submeshIndex, TexHandle _texture);
	// 画面全体へ適用するポストエフェクトmaterialを設定する(無効ハンドルを渡した場合は内蔵の素通し描画へ戻します)
	void SetPostEffect(MaterialHandle _material);
	// モデルのサブメッシュが使用するAlphaModeを変更するMaskの場合はalphaCutoff未満のピクセルを破棄する
	bool SetModelAlphaMode(ModelHandle _model, int _subMeshIndex, ModelAlphaMode _alphaMode, float _alphaCutoff = 0.5f);
	
	/// <summary>
	/// ModelLODLevelを使って作成した配列を受け取って設定した距離によってモデルを変えるLOD関数(静的モデルのみ対応)
	/// LODModelStateは内部のモデル状態を自動で変更、保持する構造体なので定義してそのまま使って下さい。
	/// HysteresisDistanceはLOD境界のちらつきを抑えるものです。近づくときは設定閾値 - HysteresisDistance、離れるときは設定閾値 + HysteresisDistanceが自動的に行われ、最終的な閾値が動的に変動します
	/// </summary>
	/// <param name="_levels">Level設定構造体の配列</param>
	/// <param name="_state">内部のLOD状態を保持する構造体</param>
	/// <param name="_transform">描画位置、回転、サイズ等を保持したTranform</param>
	/// <param name="_hysteresisDistance">境界の幅(Level構造体により設定された閾値に加えてこの値がデッドゾーン幅になります)</param>
	void DrawLODModel(std::span<const ModelLODLevel> _levels, ModelLODState& _state, const Transform& _transform, float _hysteresisDistance);

	// テクスチャリソースの解放
	void Unload(TexHandle _handle);
	// モデルリソースの開放
	void Unload(ModelHandle _handle);
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
	bool SetMaterialParameter(MaterialHandle _handle, std::size_t _slot, const T& _parameter)
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
