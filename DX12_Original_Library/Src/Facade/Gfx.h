#pragma once
#include <memory>
#include <type_traits>
#include <string_view>
#include <span>
#include "../Core/Handle/TexHandle.h"
#include "../Core/Handle/ModelHandle.h"
#include "../Core/Handle/ShaderHandle.h"
#include "../Core/Handle/MaterialHandle.h"
#include "../Core/Handle/AnimInstanceHandle.h"
#include "../Core/Handle/VFXHandle.h"
#include "../Core/Handle/VFXParticleTypeHandle.h"
#include "../Core/Handle/VFXMeshTypeHandle.h"
#include "../Graphics/GfxType.h"
#include "../Component/Transform.h"
#include "../Component/Camera.h"
#include "../Component/Light.h"
#include "../Math/TSMath.h"

struct Box; // 描画側が判定側に依存しない

// グラフィックスに関する機能をユーザーに簡易的に提供するためのファイル
namespace Gfx 
{

	// ユーザーが触ってはならない空間を名前で知らせる
	namespace Detail
	{
		// テンプレートから呼び出される内部実装
		bool SetMaterialParameterRaw(MaterialHandle _handle, std::size_t _slot , const void* _data, size_t _dataSize);
	}

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
	MaterialHandle CreateMaterial(ShaderHandle _pixel, const MaterialRenderDesc& _materialRenderDesc = {});
	// material読み込み(VSも指定する版)
	MaterialHandle CreateMaterial(ShaderHandle _vertexShader, ShaderHandle _pixelShader, const MaterialRenderDesc& _materialRenderDesc = {});
	// モデルをもとにアニメーション個体の作成
	AnimInstanceHandle CreateAnimInstance(ModelHandle _handle);

	// 名前からボーンを線形探索してそのボーン番号を返す(失敗は-1)
	int GetBoneIndex(AnimInstanceHandle _handle, const std::string_view _boneName);
	// ボーン番号からボーンのワールド行列を取得する(失敗はIdentity)
	Mat4x4 GetBoneMatrix(AnimInstanceHandle _handle, int _boneIndex, const Transform& _transform);

	// 名前からSocket(Boneに付いているnode)を線形探索してその番号を返す(失敗は-1)
	int GetSocketIndex(AnimInstanceHandle _handle, const std::string_view _socketName);
	// Socket番号からSocketのワールド行列を取得する(失敗はIdentity)
	Mat4x4 GetSocketMatrix(AnimInstanceHandle _handle, int _socketIndex, const Transform& _transform);

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

	// 単位BoxをTransformで配置して描画する
	void DrawBox3D(const Transform& _transform, Vector4 _color = {1.0f, 1.0f, 1.0f, 1.0f}, Primitive3DStyle _style = Primitive3DStyle::Fill);
	// 単位Boxを判定を使って描画する(主にデバッグ確認)
	void DrawBox3D(const Box& _box, Vector4 _color = { 0.0f, 1.0f, 0.0f, 1.0f });
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
	// Materialつきの板をワールド空間へ描画する(中心、幅と高さ、VFX用途のMaterial、向きの決め方、Noneのときの回転、頂点色)
	void DrawQuad3D(Vector3 _center, Vector2 _size, MaterialHandle _material, BillboardMode _mode = BillboardMode::FaceCamera, Quaternion _rotation = Quaternion::Identity, Vector4 _color = { 1.0f, 1.0f, 1.0f, 1.0f });
	// Materialつきの帯をワールド空間へ描画する(左の点列、右の点列、点ごとの色、VFX用途のMaterial) 3つは同じ長さで2点以上
	void DrawTrail3D(std::span<const Vector3> _left, std::span<const Vector3> _right, std::span<const Vector4> _colors, MaterialHandle _material);
	// 頂点列(TRIANGLESTRIP、3頂点以上)をそのままMaterialつきで描画する低水準API DrawQuad3D/DrawTrail3Dはこれを使う
	void DrawVFXMesh(std::span<const VFXVertex> _vertices, MaterialHandle _material);
	// VFXをメッシュ無しで描画(粒子ならこちらを使ってください)
	void DrawVFXInstanced(MaterialHandle _material, UINT _instanceCount, const Vector3& _sortPosition);
	// 平面上に3DGridを描画する(中心座標、中心から片側端までのCell数、Cell1つ分のサイズ(デフォルトは1m), 色)
	void DrawGrid3D(Vector3 _center, Quaternion _rotation = Quaternion::Identity,unsigned int _halfCellCount = 10, float _cellSize = 1.0f, Vector4 _color = { 1.0f, 1.0f, 1.0f, 1.0f });
	// ワールド空間の座標軸に沿ったグリッドを生成する(中心座標、中心から片側端までのCell数、Cell1つ分のサイズ(デフォルトは1m), 各軸のグリッドの色(デフォルトはX = 赤 Y = 緑 Z = 青です))
	void DrawWorldAxisGrid3D(Vector3 _center, unsigned int _halfCellCount = 10, float _cellSize = 1.0f, Vector4 _xAxisColor = { 1.0f, 0.0f, 0.0f, 1.0f }, Vector4 _yAxisColor = { 0.0f, 1.0f, 0.0f, 1.0f }, Vector4 _zAxisColor = { 0.0f, 0.0f, 1.0f, 1.0f });
	// ローカル座標軸を描画する
	void DrawAxis3D(Vector3 _origin, Quaternion _rotation = Quaternion::Identity, float _length = 1.0f, Vector4 _xColor = { 1.0f, 0.0f, 0.0f, 1.0f }, Vector4 _yColor = { 0.0f, 1.0f, 0.0f, 1.0f }, Vector4 _zColor = { 0.0f, 0.0f, 1.0f, 1.0f });


	// 静的モデルを描画する
	void DrawModel(ModelHandle _model, Transform _transform);
	// 静的モデルをmaterialつきで描画する
	void DrawModel(ModelHandle _model, Transform _transform, MaterialHandle _material);
	// スキニングモデルを描画する
	void DrawAnimatedModel(AnimInstanceHandle _handle, Transform _transform);
	// スキニングモデルをmaterialつきで描画する
	void DrawAnimatedModel(AnimInstanceHandle _handle, Transform _transform, MaterialHandle _material);
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

	// 粒子VFXの種類を登録する materialはテンプレートでライブラリが同時再生数ぶん複製し枠ごとの粒子Bufferをslot0へ結びslot3へ時間を書く
	VFXParticleTypeHandle RegisterVFXParticle(const VFXParticleDesc& _desc);
	// 再生開始する
	VFXHandle PlayVFX(VFXParticleTypeHandle _particleType, const Vector3& _position);
	// 毎フレーム呼ぶ 経過を進めてComputeと描画の登録を行う 終了した更新でfalse(dt = 0 ならヒットストップ)
	bool UpdateVFX(VFXHandle _handle, float _deltaTime);
	// 発生位置を動かす 発生速度は位置の差分からライブラリが出す
	bool SetVFXPosition(VFXHandle _handle, const Vector3& _position);
	// 終了していればtrue(無効ハンドル 古いハンドルもtrue)
	bool IsFinishedVFX(VFXHandle _handle);
	// 既定は発生だけ止めて余韻を残す _immediateなら全粒子を消して枠を返す
	bool StopVFX(VFXHandle _handle, bool _immediate = false);
	// メッシュVFXの種類を登録する materialはテンプレートでライブラリが同時再生数ぶん複製しslot3へ時間を書く
	VFXMeshTypeHandle RegisterVFXMesh(const VFXMeshDesc& _desc);
	// メッシュVFXを再生開始する 板は位置と向きを使う 帯は使わない
	VFXHandle PlayVFX(VFXMeshTypeHandle _meshType, const Vector3& _position, const Quaternion& _rotation = Quaternion::Identity);
	// 帯VFXへこのフレームの左右の点を1組渡す
	bool SetVFXTrailPoint(VFXHandle _handle, const Vector3& _left, const Vector3& _right);
	
	/// <summary>
	/// <para>ModelLODLevelを使って作成した配列を受け取って設定した距離によってモデルを変えるLOD関数(静的モデルのみ対応)</para>
	/// <para>LODModelStateは内部のモデル状態を自動で変更、保持する構造体なので定義してそのまま使って下さい。</para>
	/// <para>HysteresisDistanceはLOD境界のちらつきを抑えるものです。近づくときは設定閾値 - HysteresisDistance、離れるときは設定閾値 + HysteresisDistanceが自動的に行われ、最終的な閾値が動的に変動します</para>
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
	/// <para>任意のマテリアルに対して任意のslotにパラメータを設定する</para>
	/// <para>パラメータは最大4つまで設定できます</para>
	/// <para>ユーザーが使えるregister spaceは1番なので定数バッファ宣言の後にはspace1を入れてください</para>
	/// <para>定義例 : cbuffer Example : register(b0, space1)</para>
	/// <para>レジスタの0-3まで置かれるので0を指定したらHLSL側ではb0, 1ならb1...となりb3まで使えます</para>
	/// <para>PlayVFXで再生するmaterialはslot3(b3)へライブラリが毎フレーム時間を書くので、slot3に設定した値は上書きされます(使えるのはslot0-2です)</para>
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
	/// <para>任意のマテリアルに対してパラメータを設定する</para>
	/// <para>スロットを指定していないので0として扱い、b0に配置されます</para>
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

	/// <summary>
	/// <\para>任意のmaterialに対してテクスチャを設定する<\para>
	/// <\para>テクスチャは最大4まで設定できます<\para>
	/// <para>ユーザーが使えるregister spaceは1番なのでTexture2D宣言の後にはspace1を入れてください</para>
	/// <para>定義例 : Texture2D Example : register(t0, space1)</para>
	/// <para>レジスタの0-3まで置かれるので0を指定したらHLSL側ではt0, 1ならt1...となりt3まで使えます</para>
	/// </summary>
	/// <param name="_material">設定したいmaterial</param>
	/// <param name="_slot">設定するスロット</param>
	/// <param name="_texture">入れるtexture</param>
	/// <returns>成功したか</returns>
	bool SetMaterialTexture(MaterialHandle _material, std::size_t _slot, TexHandle _texture);

	/// <summary>
	/// <\para>任意のmaterialに対してテクスチャを設定する<\para>
	/// <\para>テクスチャは最大4まで設定できます<\para>
	/// <\para>この関数ではスロットを指定していないので0が使われます<\para>
	/// <para>ユーザーが使えるregister spaceは1番なのでTexture2D宣言の後にはspace1を入れてください</para>
	/// <para>定義例 : Texture2D Example : register(t0, space1)</para>
	/// </summary>
	/// <param name="_material">設定したいmaterial</param>
	/// <param name="_texture">入れるtexture</param>
	/// <returns>成功したか</returns>
	bool SetMaterialTexture(MaterialHandle _material, TexHandle _texture);

	/// <summary>
	/// <para>任意のmaterialに対してComputeで作ったBufferを設定する</para>
	/// <para>TextureとBufferは同じ4つのスロット(t0〜t3, space1)を共有し、1つのスロットにはどちらか一方しか置けません。後から設定した方が有効になります</para>
	/// <para>HLSL側ではStructuredBufferとして宣言し要素の型はCreateGPUBuffer時のelementStrideと同じ大きさにしてください</para>
	/// <para>定義例 : StructuredBuffer&lt;Particle&gt; particles : register(t0, space1)</para>
	/// <para>Compute側で書き込んだBufferはEndFrame内のCompute実行後に読み取り状態へ移されるので同じフレームのDispatch結果をそのまま描画で読めます</para>
	/// </summary>
	/// <param name="_material">設定したいmaterial</param>
	/// <param name="_slot">設定するSlot</param>
	/// <param name="_gpuBuffer">入れるBuffer</param>
	/// <returns>成功したか</returns>
	bool SetMaterialBuffer(MaterialHandle _material, std::size_t _slot, GPUBufferHandle _gpuBuffer);

	/// <summary>
	/// <para>任意のmaterialに対してComputeで作ったBufferを設定する</para>
	/// <para>TextureとBufferは同じ4つのスロット(t0〜t3, space1)を共有し、1つのスロットにはどちらか一方しか置けません。後から設定した方が有効になります</para>
	/// <para>HLSL側ではStructuredBufferとして宣言し要素の型はCreateGPUBuffer時のelementStrideと同じ大きさにしてください</para>
	/// <para>定義例 : StructuredBuffer&lt;Particle&gt; particles : register(t0, space1)</para>
	/// <para>Compute側で書き込んだBufferはEndFrame内のCompute実行後に読み取り状態へ移されるので同じフレームのDispatch結果をそのまま描画で読めます</para>
	/// <para> この関数ではslotを省略して自動的に0番に置きます</para>
	/// </summary>
	/// <param name="_material">設定したいmaterial</param>
	/// <param name="_gpuBuffer">入れるBuffer</param>
	/// <returns></returns>
	bool SetMaterialBuffer(MaterialHandle _material, GPUBufferHandle _gpuBuffer);
}
