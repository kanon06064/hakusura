#include "Enemy.h"
#include "Player.h"
#include "Dungeon.h"
#include "EffectManager.h"
#include <math.h>

Enemy::Enemy(Vector3 sp, EnemyData d, int fl) {
    position = sp; data = d; eType = (EnemyType)d.type; state = STATE_PATROL; level = fl;
    maxHp = d.hp + (level * 10.0f); hp = maxHp; speed = d.speed; detectRange = d.detect; attackRange = d.atkRange; expValue = d.exp;
    radius = 0.45f; attackTimer = 0; hudTimer = 0;
    lastAttackDir = { 1,0,0 };
    patrolTarget = sp;
}

void Enemy::ApplyKnockback(Vector3 dir, float f, Dungeon& d) {
    Vector3 kb = Vector3Scale(dir, f);
    if (!d.CheckCollisionRadius(Vector3Add(position, { kb.x, 0, 0 }), radius)) position.x += kb.x;
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, kb.z }), radius)) position.z += kb.z;
}

void Enemy::MoveSmart(Vector3 target, Dungeon& d) {
    Vector3 dir = Vector3Normalize(Vector3Subtract(target, position));
    Vector3 vel = Vector3Scale(dir, speed);
    if (!d.CheckCollisionRadius(Vector3Add(position, { vel.x, 0, 0 }), radius)) position.x += vel.x;
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, vel.z }), radius)) position.z += vel.z;
}

void Enemy::Update(Player& p, Dungeon& d, EffectManager& fx) {
    if (hudTimer > 0) hudTimer -= GetFrameTime();
    if (attackTimer > 0) attackTimer -= GetFrameTime();

    float dist = Vector3Distance(position, p.position);
    bool canSee = d.HasLineOfSight(position, p.position);

    if (dist < attackRange && canSee) state = STATE_ATTACK;
    else if (dist < detectRange && canSee) state = STATE_CHASE;
    else state = STATE_PATROL;

    if (state == STATE_CHASE || state == STATE_ATTACK) {
        if (dist > attackRange * 0.7f) MoveSmart(p.position, d);

        if (dist < attackRange && attackTimer <= 0) {
            lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));

            // 攻撃発生位置
            Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });

            if (eType == E_ARCHER) {
                fx.SpawnProjectile(spawnPos, lastAttackDir, 18.0f, 0, false);
                attackTimer = 1.8f;
            }
            else if (eType == E_MAGE) {
                fx.SpawnProjectile(spawnPos, lastAttackDir, 8.0f, 1, false);
                attackTimer = 2.2f;
            }
            else {
                // 近接攻撃 (直接ダメージ + エフェクト)
                float rawDmg = 10.0f + level * 2;
                float dmg = fmaxf(1.0f, rawDmg - p.defense);
                p.hp -= dmg;

                fx.SpawnDamageText(p.position, (int)dmg);
                fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED); // プレイヤー位置でヒットエフェクト

                attackTimer = 1.5f;
            }
        }
    }
    else {
        if (Vector3Distance(position, patrolTarget) < 1.2f) patrolTarget = d.GetRandomFloorPos();
        MoveSmart(patrolTarget, d);
    }
}

void Enemy::Draw(bool debug) {
    Color c = (eType == E_SWORD) ? MAROON : (eType == E_SPEAR) ? ORANGE : (eType == E_AXE) ? PURPLE : (eType == E_ARCHER) ? DARKGREEN : (eType == E_MAGE) ? PINK : GRAY;
    DrawCube(position, 1, 1.2f, 1, c); DrawCubeWires(position, 1, 1.2f, 1, BLACK);

    if (debug) {
        DrawCircle3D(position, data.atkRange, { 1,0,0 }, 90.0f, Fade(RED, 0.25f));
        DrawCircle3D(position, data.detect, { 1,0,0 }, 90.0f, Fade(YELLOW, 0.1f));
        DrawLine3D(position, patrolTarget, GREEN);
    }
}