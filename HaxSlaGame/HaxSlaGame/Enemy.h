#pragma once
#include "Definitions.h"

class Player; class Dungeon;

class Enemy {
public:
    Vector3 position, lastAttackDir, lastPos, patrolTarget;
    EnemyState state; EnemyType eType; EnemyData data;
    float hp, maxHp, speed, radius, detectRange, attackRange, attackTimer, hudTimer, visualTimer;
    int stuckFrames, level, expValue;

    Enemy(Vector3 startPos, EnemyData data, int floorLevel);
    void Update(Player& player, Dungeon& dungeon, std::vector<Projectile>& enemyProj);
    void Draw();
    void ApplyKnockback(Vector3 dir, float force, Dungeon& dungeon);
private:
    void MoveSmart(Vector3 target, Dungeon& dungeon);
};