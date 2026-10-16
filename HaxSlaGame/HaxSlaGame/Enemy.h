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

    // ★ defense (防御力) を追加
    float hp, maxHp, defense, speed, radius, detectRange, attackRange, attackTimer, hudTimer;
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
    int normalAttackPattern;

    bool isBoss;
    int bossAttackType;
    int bossComboStep;
    float bossActionTimer;
    Vector3 bossTargetDir;

    float delayedHp;
    float delayedHpTimer;

    Enemy(Vector3 sp, EnemyData d, int fl);

    bool IsPlant() const;

    // ★ プレイヤーへの被ダメージ計算（漸近防御減衰式）
    float CalculateDamageToPlayer(const Player& p, float rawDmg, int attackElem);

    void Update(Player& p, Dungeon& d, EffectManager& fx);
    void Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos);
    void ApplyKnockback(Vector3 dir, float force, Dungeon& d);

    void StartDeath();

private:
    bool MoveSmart(Vector3 t, Dungeon& d);
};