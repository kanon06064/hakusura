#include "Enemy.h"
#include "Player.h"
#include "Dungeon.h"
#include "raymath.h"

Enemy::Enemy(Vector3 startPos, EnemyType t) {
    position = startPos; type = t; state = STATE_PATROL;
    maxHp = (t == E_TRAP) ? 60.0f : 30.0f; hp = maxHp;
    radius = 0.45f; attackTimer = 0; hudTimer = 0; visualTimer = 0;
    lastAttackDir = { 1, 0, 0 }; lastPos = startPos; stuckFrames = 0;

    switch (type) {
    case E_SWORD:  speed = 0.07f; detectRange = 12.0f; attackRange = 2.2f; break;
    case E_SPEAR:  speed = 0.06f; detectRange = 12.0f; attackRange = 4.5f; break;
    case E_AXE:    speed = 0.09f; detectRange = 12.0f; attackRange = 2.6f; break;
    case E_ARCHER: speed = 0.05f; detectRange = 15.0f; attackRange = 12.0f; break;
    case E_MAGE:   speed = 0.04f; detectRange = 12.0f; attackRange = 10.0f; break;
    case E_TRAP:   speed = 0.00f; detectRange = 10.0f; attackRange = 10.0f; break;
    }
    patrolTarget = startPos;
}

void Enemy::ApplyKnockback(Vector3 dir, float force, Dungeon& dungeon) {
    Vector3 kb = Vector3Scale(dir, force);
    if (!dungeon.CheckCollisionRadius({ position.x + kb.x, 0.5f, position.z }, radius)) position.x += kb.x;
    if (!dungeon.CheckCollisionRadius({ position.x, 0.5f, position.z + kb.z }, radius)) position.z += kb.z;
}

void Enemy::MoveSmart(Vector3 target, Dungeon& dungeon) {
    Vector3 dir = Vector3Normalize(Vector3Subtract(target, position));
    Vector3 vel = Vector3Scale(dir, speed);

    bool movedX = false;
    bool movedZ = false;

    if (!dungeon.CheckCollisionRadius({ position.x + vel.x, 0.5f, position.z }, radius)) {
        position.x += vel.x; movedX = true;
    }
    if (!dungeon.CheckCollisionRadius({ position.x, 0.5f, position.z + vel.z }, radius)) {
        position.z += vel.z; movedZ = true;
    }

    // スタック判定：目的地があるのに動けていない場合
    if (state == STATE_PATROL && (!movedX || !movedZ)) {
        stuckFrames++;
        if (stuckFrames > 10) {
            // 左右に少しずれてみる
            float ang = (stuckFrames % 2 == 0) ? 45.0f : -45.0f;
            Vector3 detourDir = Vector3RotateByAxisAngle(dir, { 0, 1, 0 }, ang * DEG2RAD);
            Vector3 detourVel = Vector3Scale(detourDir, speed);
            if (!dungeon.CheckCollisionRadius(Vector3Add(position, detourVel), radius)) position = Vector3Add(position, detourVel);

            if (stuckFrames > 30) { // それでもダメなら諦めて別地点へ
                patrolTarget = dungeon.GetRandomFloorPos();
                stuckFrames = 0;
            }
        }
    }
    else stuckFrames = 0;
}

void Enemy::Update(Player& player, Dungeon& dungeon, std::vector<Projectile>& enemyProj) {
    if (hudTimer > 0) hudTimer -= GetFrameTime();
    if (attackTimer > 0) attackTimer -= GetFrameTime();
    if (visualTimer > 0) visualTimer -= GetFrameTime();

    lastPos = position;
    float dist = Vector3Distance(position, player.position);
    bool canSee = dungeon.HasLineOfSight(position, player.position);

    if (type == E_TRAP) {
        if (canSee && dist < attackRange && attackTimer <= 0) {
            for (int i = 0; i < 8; i++) {
                float a = i * 45.0f * DEG2RAD;
                enemyProj.push_back({ position, Vector3Scale({cosf(a), 0, sinf(a)}, 6.0f), 0.3f, true, 1 });
            }
            attackTimer = 3.0f; visualTimer = 0.2f;
        }
        return;
    }

    if (dist < attackRange && canSee) state = STATE_ATTACK;
    else if (dist < detectRange && canSee) state = STATE_CHASE;
    else state = STATE_PATROL;

    if (state == STATE_CHASE || state == STATE_ATTACK) {
        if ((type == E_ARCHER || type == E_MAGE) && dist < 6.0f) { /* 距離維持 */ }
        else if (dist > attackRange * 0.7f) MoveSmart(player.position, dungeon);

        if (dist < attackRange && attackTimer <= 0) {
            lastAttackDir = Vector3Normalize(Vector3Subtract(player.position, position));
            visualTimer = 0.25f;

            if (type == E_ARCHER) { enemyProj.push_back({ position, Vector3Scale(lastAttackDir, 18.0f), 0.2f, true, 0 }); attackTimer = 1.8f; }
            else if (type == E_MAGE) { enemyProj.push_back({ position, Vector3Scale(lastAttackDir, 8.0f), 0.6f, true, 1 }); attackTimer = 2.5f; }
            else {
                // 近接攻撃判定
                bool hit = false;
                float knock = 0.6f;
                if (type == E_AXE) {
                    Vector3 center = Vector3Add(position, Vector3Scale(lastAttackDir, 2.0f));
                    if (Vector3Distance(player.position, center) < 2.5f) { hit = true; knock = 1.0f; }
                }
                else if (type == E_SPEAR) {
                    Vector3 v = Vector3Subtract(player.position, position);
                    float fDist = Vector3DotProduct(v, lastAttackDir);
                    float sDist = fabsf(Vector3DotProduct(v, { -lastAttackDir.z, 0, lastAttackDir.x }));
                    if (fDist > 0 && fDist < 4.5f && sDist < 0.6f) { hit = true; knock = 1.4f; }
                }
                else {
                    Vector3 v = Vector3Subtract(player.position, position);
                    if (dist < 2.5f && Vector3DotProduct(lastAttackDir, Vector3Normalize(v)) > cosf(60 * DEG2RAD)) { hit = true; knock = 0.6f; }
                }

                if (hit) {
                    player.hp -= 10.0f;
                    Vector3 pkb = Vector3Scale(lastAttackDir, knock);
                    if (!dungeon.CheckCollisionRadius({ player.position.x + pkb.x, 0.5f, player.position.z }, player.radius)) player.position.x += pkb.x;
                    if (!dungeon.CheckCollisionRadius({ player.position.x, 0.5f, player.position.z + pkb.z }, player.radius)) player.position.z += pkb.z;
                }
                attackTimer = 1.5f;
            }
        }
    }
    else {
        if (Vector3Distance(position, patrolTarget) < 1.2f) patrolTarget = dungeon.GetRandomFloorPos();
        MoveSmart(patrolTarget, dungeon);
    }
}

void Enemy::Draw() {
    Color c = (type == E_SWORD) ? MAROON : (type == E_SPEAR) ? ORANGE : (type == E_AXE) ? PURPLE : (type == E_ARCHER) ? DARKBLUE : (type == E_MAGE) ? PINK : GRAY;
    DrawCube(position, 1.0f, (type == E_TRAP ? 2.0f : 1.2f), 1.0f, c);
    DrawCubeWires(position, 1.0f, (type == E_TRAP ? 2.0f : 1.2f), 1.0f, BLACK);

    if (visualTimer > 0) {
        float y = 0.2f;
        if (type == E_SWORD) {
            float base = atan2f(lastAttackDir.z, lastAttackDir.x);
            for (int i = -60; i <= 60; i += 15) DrawLine3D({ position.x, y, position.z }, { position.x + cosf(base + (i * DEG2RAD)) * 2.5f, y, position.z + sinf(base + (i * DEG2RAD)) * 2.5f }, RED);
        }
        else if (type == E_SPEAR) {
            Vector3 r = { -lastAttackDir.z, 0, lastAttackDir.x };
            Vector3 p3 = Vector3Add(position, Vector3Subtract(Vector3Scale(lastAttackDir, 4.5f), Vector3Scale(r, 0.5f)));
            Vector3 p4 = Vector3Add(position, Vector3Add(Vector3Scale(lastAttackDir, 4.5f), Vector3Scale(r, 0.5f)));
            p3.y = p4.y = y;
            DrawLine3D({ position.x, y, position.z }, p3, RED); DrawLine3D({ position.x, y, position.z }, p4, RED); DrawLine3D(p3, p4, RED);
        }
        else if (type == E_AXE) {
            Vector3 center = Vector3Add(position, Vector3Scale(lastAttackDir, 2.0f));
            DrawSphereWires({ center.x, y, center.z }, 2.5f, 8, 8, RED);
        }
        else if (type == E_TRAP) DrawCircleLinesV({ position.x, position.z }, 6.0f, RED);
    }
}