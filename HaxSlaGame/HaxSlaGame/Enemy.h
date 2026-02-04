#pragma once
#include "Definitions.h"
#include "raymath.h"

// ‘O•ûéŒ¾
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

    Enemy(Vector3 sp, EnemyData d, int fl);

    // EffectManager‚ğó‚¯æ‚é‚æ‚¤‚É•ÏX
    void Update(Player& p, Dungeon& d, EffectManager& fx);
    void Draw(bool debug);
    void ApplyKnockback(Vector3 dir, float force, Dungeon& d);

private:
    void MoveSmart(Vector3 t, Dungeon& d);
};