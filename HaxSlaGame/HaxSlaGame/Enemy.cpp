#include "Enemy.h"
#include "Player.h"
#include "Dungeon.h"
#include "raymath.h"

Enemy::Enemy(Vector3 startPos, EnemyData d, int floorLevel) {
    position = startPos; data = d; type = (EnemyType)d.type; state = STATE_PATROL; level = floorLevel;
    maxHp = d.hp + (level * 10.0f); hp = maxHp; speed = d.speed; detectRange = d.detect; attackRange = d.atkRange; expValue = d.exp;
    radius = 0.45f; attackTimer = 0; hudTimer = 0; visualTimer = 0; stuckFrames = 0;
    lastAttackDir = { 1,0,0 }; lastPos = startPos; patrolTarget = startPos;
}
void Enemy::ApplyKnockback(Vector3 dir, float f, Dungeon& d) {
    Vector3 kb = Vector3Scale(dir, f);
    if (!d.CheckCollisionRadius(Vector3Add(position, { kb.x, 0, 0 }), radius)) position.x += kb.x;
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, kb.z }), radius)) position.z += kb.z;
}
void Enemy::MoveSmart(Vector3 target, Dungeon& d) {
    Vector3 dir = Vector3Normalize(Vector3Subtract(target, position));
    Vector3 vel = Vector3Scale(dir, speed);
    bool mx = !d.CheckCollisionRadius(Vector3Add(position, { vel.x, 0, 0 }), radius);
    bool mz = !d.CheckCollisionRadius(Vector3Add(position, { 0, 0, vel.z }), radius);
    if (mx) position.x += vel.x; if (mz) position.z += vel.z;
    if (state == STATE_PATROL && (!mx || !mz)) {
        stuckFrames++;
        if (stuckFrames > 10) {
            Vector3 detour = Vector3RotateByAxisAngle(dir, { 0,1,0 }, (stuckFrames % 2 == 0 ? 45.0f : -45.0f) * DEG2RAD);
            if (!d.CheckCollisionRadius(Vector3Add(position, Vector3Scale(detour, speed)), radius)) position = Vector3Add(position, Vector3Scale(detour, speed));
            if (stuckFrames > 30) { patrolTarget = d.GetRandomFloorPos(); stuckFrames = 0; }
        }
    }
    else stuckFrames = 0;
}
void Enemy::Update(Player& p, Dungeon& d, std::vector<Projectile>& proj) {
    if (hudTimer > 0) hudTimer -= GetFrameTime();
    if (attackTimer > 0) attackTimer -= GetFrameTime();
    if (visualTimer > 0) visualTimer -= GetFrameTime();
    float dist = Vector3Distance(position, p.position);
    bool canSee = d.HasLineOfSight(position, p.position);
    if (type == E_TRAP) {
        if (canSee && dist < attackRange && attackTimer <= 0) {
            for (int i = 0; i < 8; i++) proj.push_back({ position, Vector3Scale({cosf((float)i * 45 * DEG2RAD),0,sinf((float)i * 45 * DEG2RAD)}, 6), 0.3f, true, 1 });
            attackTimer = 3.0f; visualTimer = 0.2f;
        }
        return;
    }
    if (dist < attackRange && canSee) state = STATE_ATTACK;
    else if (dist < detectRange && canSee) state = STATE_CHASE;
    else state = STATE_PATROL;
    if (state == STATE_CHASE || state == STATE_ATTACK) {
        if (dist > attackRange * 0.7f) MoveSmart(p.position, d);
        if (dist < attackRange && attackTimer <= 0) {
            lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position)); visualTimer = 0.25f;
            if (type == E_ARCHER) { proj.push_back({ position, Vector3Scale(lastAttackDir, 18), 0.2f, true, 0 }); attackTimer = 1.8f; }
            else if (type == E_MAGE) { proj.push_back({ position, Vector3Scale(lastAttackDir, 8), 0.6f, true, 1 }); attackTimer = 2.2f; }
            else {
                float k = (type == E_SPEAR) ? 1.4f : 0.6f;
                p.hp -= fmaxf(1.0f, (10.0f + (float)level * 2.0f) - p.defense);
                Vector3 pkb = Vector3Scale(lastAttackDir, k);
                if (!d.CheckCollisionRadius(Vector3Add(p.position, { pkb.x,0,0 }), p.radius)) p.position.x += pkb.x;
                if (!d.CheckCollisionRadius(Vector3Add(p.position, { 0,0,pkb.z }), p.radius)) p.position.z += pkb.z;
                attackTimer = 1.5f;
            }
        }
    }
    else {
        if (Vector3Distance(position, patrolTarget) < 1.2f) patrolTarget = d.GetRandomFloorPos();
        MoveSmart(patrolTarget, d);
    }
}
void Enemy::Draw() {
    Color c = (type == E_SWORD) ? MAROON : (type == E_SPEAR) ? ORANGE : (type == E_AXE) ? PURPLE : (type == E_ARCHER) ? DARKBLUE : (type == E_MAGE) ? PINK : GRAY;
    DrawCube(position, 1, (type == E_TRAP ? 2 : 1.2f), 1, c);
    DrawCubeWires(position, 1, (type == E_TRAP ? 2 : 1.2f), 1, BLACK);
    if (visualTimer > 0) {
        float b = atan2f(lastAttackDir.z, lastAttackDir.x);
        if (type == E_SWORD) { for (int i = -60; i <= 60; i += 15) DrawLine3D(position, Vector3Add(position, Vector3Scale({ cosf(b + (float)i * DEG2RAD), 0, sinf(b + (float)i * DEG2RAD) }, 2.5f)), RED); }
        else if (type == E_AXE) DrawSphereWires(Vector3Add(position, Vector3Scale(lastAttackDir, 2)), 2.5f, 8, 8, RED);
        else if (type == E_SPEAR) {
            Vector3 r = { -lastAttackDir.z, 0, lastAttackDir.x }, p3 = Vector3Add(position, Vector3Subtract(Vector3Scale(lastAttackDir, 4.5f), Vector3Scale(r, 0.5f))), p4 = Vector3Add(position, Vector3Add(Vector3Scale(lastAttackDir, 4.5f), Vector3Scale(r, 0.5f)));
            DrawLine3D(position, p3, RED); DrawLine3D(position, p4, RED); DrawLine3D(p3, p4, RED);
        }
    }
}