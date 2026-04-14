#pragma once
#include "Definitions.h"
#include "raymath.h"

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

    Vector3 lastPos;
    int stuckCount;

    // アニメーション管理用
    int animFrameCounter;
    bool isDying;
    bool isDead;

    // ★ ボス専用AIフラグと状態管理（この部分が不足してエラーになっていました）
    bool isBoss;
    int bossAttackType;
    int bossComboStep;
    float bossActionTimer;
    Vector3 bossTargetDir;

    Enemy(Vector3 sp, EnemyData d, int fl);

    void Update(Player& p, Dungeon& d, EffectManager& fx);
    void Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos);
    void ApplyKnockback(Vector3 dir, float force, Dungeon& d);

    void StartDeath();

private:
    bool MoveSmart(Vector3 t, Dungeon& d);
};