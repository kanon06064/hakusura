#include "Enemy.h"
#include "Player.h"
#include "Dungeon.h"
#include "raymath.h"

Enemy::Enemy(Vector3 startPos) {
    position = startPos;
    state = STATE_PATROL;
    speed = 0.07f;
    radius = 0.45f;
    detectRange = 12.0f;
    attackRange = 1.8f;
    patrolTarget = startPos;
}

void Enemy::Update(Player& player, Dungeon& dungeon) {
    float dist = Vector3Distance(position, player.position);
    bool canSeePlayer = (dist < detectRange) && dungeon.HasLineOfSight(position, player.position);

    if (dist < attackRange) state = STATE_ATTACK;
    else if (canSeePlayer) state = STATE_CHASE;
    else state = STATE_PATROL;

    switch (state) {
    case STATE_PATROL:
        if (Vector3Distance(position, patrolTarget) < 1.0f) patrolTarget = dungeon.GetRandomFloorPos();
        MoveTowards(patrolTarget, dungeon);
        break;
    case STATE_CHASE:
        MoveTowards(player.position, dungeon);
        break;
    case STATE_ATTACK: break;
    }
}

void Enemy::MoveTowards(Vector3 target, Dungeon& dungeon) {
    Vector3 diff = Vector3Subtract(target, position);
    diff.y = 0;
    if (Vector3Length(diff) < 0.1f) return;
    Vector3 dir = Vector3Normalize(diff);
    Vector3 velocity = Vector3Scale(dir, speed);

    Vector3 nextPosX = { position.x + velocity.x, position.y, position.z };
    if (!dungeon.CheckCollisionRadius(nextPosX, radius)) position.x = nextPosX.x;
    else if (state == STATE_PATROL) patrolTarget = dungeon.GetRandomFloorPos();

    Vector3 nextPosZ = { position.x, position.y, position.z + velocity.z };
    if (!dungeon.CheckCollisionRadius(nextPosZ, radius)) position.z = nextPosZ.z;
    else if (state == STATE_PATROL) patrolTarget = dungeon.GetRandomFloorPos();
}

void Enemy::Draw() {
    Color c = (state == STATE_CHASE) ? ORANGE : (state == STATE_ATTACK) ? YELLOW : PURPLE;
    DrawCube(position, 1.0f, 1.2f, 1.0f, c);
    DrawCubeWires(position, 1.0f, 1.2f, 1.0f, BLACK);
}