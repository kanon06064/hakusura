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

    // 死亡アニメーション管理用
    bool isDying;
    bool isDead;

    Enemy(Vector3 sp, EnemyData d, int fl);

    void Update(Player& p, Dungeon& d, EffectManager& fx);
    void Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos);
    void ApplyKnockback(Vector3 dir, float force, Dungeon& d);

    void StartDeath();

private:
    // 【変更】壁に当たったかどうかを返すように bool に変更
    bool MoveSmart(Vector3 t, Dungeon& d);
};