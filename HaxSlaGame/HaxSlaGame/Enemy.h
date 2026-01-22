#pragma once
#include "raylib.h"
#include <vector>

class Dungeon;
class Player;

// 弾（プロジェクトタイル）の構造体
struct Projectile {
    Vector3 pos, vel;
    float radius;
    bool active;
    int type; // 0: Arrow, 1: Magic
};

enum EnemyType { E_SWORD, E_SPEAR, E_AXE, E_ARCHER, E_MAGE, E_TRAP };
enum EnemyState { STATE_PATROL, STATE_CHASE, STATE_ATTACK };

class Enemy {
public:
    Vector3 position, lastAttackDir, lastPos;
    EnemyState state;
    EnemyType type;
    float hp, maxHp, speed, radius, detectRange, attackRange;
    float attackTimer, hudTimer;
    float visualTimer; // ← これが必要な識別子です
    int stuckFrames;
    Vector3 patrolTarget;

    Enemy(Vector3 startPos, EnemyType t);
    void Update(Player& player, Dungeon& dungeon, std::vector<Projectile>& enemyProj);
    void Draw();
    void ApplyKnockback(Vector3 dir, float force, Dungeon& dungeon);

private:
    void MoveSmart(Vector3 target, Dungeon& dungeon);
};