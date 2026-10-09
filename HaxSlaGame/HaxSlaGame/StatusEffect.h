#pragma once
#include "raylib.h"
#include <string>

enum StatusType {
    STATUS_NONE = 0,
    STATUS_BURN,     // 業火：炎上（持続ダメージ）
    STATUS_ROT,      // 腐蝕：溶解（防御力低下）
    STATUS_CHILL,    // 霊怨：凍傷（移動速度低下）
    STATUS_CURSE     // 深淵：衰弱（敵攻撃力低下）
};

struct StatusData {
    StatusType type = STATUS_NONE;
    float timer = 0.0f;     // 残り持続時間
    float maxTimer = 0.0f;
    float tickTimer = 0.0f; // 継続ダメージのインターバル
    float power = 0.0f;     // ダメージ量またはデバフ率
};

class StatusManager {
public:
    StatusData burn;
    StatusData rot;
    StatusData chill;
    StatusData curse;

    // 属性から状態異常を付与
    void ApplyStatus(int element, float duration, float power);

    // 毎フレームの更新（Dotダメージの発生など）
    void Update(float dt, float& currentHp, class EffectManager& fx, Vector3 pos);

    // 敵の足元にオーラ/パーティクルを描画
    void DrawAura(Vector3 pos, float radius);

    // デバフ倍率の取得
    float GetSpeedMultiplier() const;      // 移動速度倍率（凍傷時 0.5倍）
    float GetDefenseReductionRate() const; // 防御低下率（溶解時 0.30倍）
    float GetAttackReductionRate() const;  // 攻撃力低下率（衰弱時 0.25倍）

    bool HasAnyStatus() const;
};