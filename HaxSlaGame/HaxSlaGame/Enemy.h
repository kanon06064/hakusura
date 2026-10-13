#pragma once
#include "Definitions.h"
#include "raymath.h"
#include "StatusEffect.h"

class Player;
class Dungeon;
class EffectManager;

class Enemy {
public:
    Vector3 position, lastAttackDir, patrolTarget;
    EnemyState state;
    EnemyType eType;
    EnemyData data;
    float hp, maxHp, speed, radius, detectRange, attackRange, attackTimer, hudTimer;
    int level, expValue;
    StatusManager status;

    Vector3 lastPos;
    int stuckCount;

    int animFrameCounter;
    float attackAnimTimer;
    bool isDying;
    bool isDead;

    bool isChargingAttack;
    float attackChargeTimer;
    float attackChargeMax;
    int normalAttackPattern; // ★ 通常敵の遠距離攻撃バリエーション番号

    bool isBoss;
    int bossAttackType;      // 1:扇状弾幕, 2:全方位スパイラル, 3:毒沼設置, 4:巨大範囲毒爆発
    int bossComboStep;
    float bossActionTimer;
    Vector3 bossTargetDir;

    float delayedHp;
    float delayedHpTimer;

    Enemy(Vector3 sp, EnemyData d, int fl);

    // ★ 植物系モンスター（ポイズンプラント、魔界樹など）の自動判定
    bool IsPlant() const;

    void Update(Player& p, Dungeon& d, EffectManager& fx);
    void Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos);
    void ApplyKnockback(Vector3 dir, float force, Dungeon& d);

    void StartDeath();

private:
    bool MoveSmart(Vector3 t, Dungeon& d);
};