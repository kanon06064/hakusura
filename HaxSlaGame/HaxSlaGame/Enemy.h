#pragma once
#include "raylib.h"

class Dungeon;
class Player;

enum EnemyState { STATE_PATROL, STATE_CHASE, STATE_ATTACK };

class Enemy {
public:
    Vector3 position;
    EnemyState state;
    float speed;
    float radius;
    float detectRange;
    float attackRange;
    Vector3 patrolTarget;

    Enemy(Vector3 startPos);
    void Update(Player& player, Dungeon& dungeon);
    void Draw();

private:
    void MoveTowards(Vector3 target, Dungeon& dungeon);
};