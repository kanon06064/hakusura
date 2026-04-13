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

    // ★追加: ボス専用AIフラグと状態管理
    bool isBoss;
    int bossAttackType; // 0:なし, 1:近接3段, 2:遠距離3way, 3:突進, 4:広範囲AoE
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