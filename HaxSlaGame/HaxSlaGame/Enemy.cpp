#include "Enemy.h"
#include "Player.h"
#include "Dungeon.h"
#include "EffectManager.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "rlgl.h"
#include <math.h>
#include <string>
#include <iostream>

Enemy::Enemy(Vector3 sp, EnemyData d, int fl) {
    position = sp; data = d; eType = (EnemyType)d.type; state = STATE_PATROL; level = fl;

    // パラメータ計算
    maxHp = d.hp + (level * 10.0f);
    hp = maxHp;
    speed = d.speed;
    detectRange = d.detect;
    attackRange = d.atkRange;

    // 階層に応じて経験値を増加させる
    int levelBonus = (int)((float)d.exp * 0.1f * (float)level) + level;
    expValue = d.exp + levelBonus;

    radius = 0.45f; attackTimer = 0; hudTimer = 0;
    lastAttackDir = { 1,0,0 };
    patrolTarget = sp;

    lastPos = sp;
    stuckCount = 0;
    animFrameCounter = GetRandomValue(0, 30);

    isDying = false;
    isDead = false;
}

void Enemy::StartDeath() {
    if (isDying) return;
    isDying = true;
    animFrameCounter = 0;

    std::string key = data.modelName;
    if (key.empty() || DataManager::loadedModels.count(key) == 0) {
        isDead = true;
    }
}

void Enemy::ApplyKnockback(Vector3 dir, float f, Dungeon& d) {
    if (isDying) return;
    Vector3 kb = Vector3Scale(dir, f);
    if (!d.CheckCollisionRadius(Vector3Add(position, { kb.x, 0, 0 }), radius)) position.x += kb.x;
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, kb.z }), radius)) position.z += kb.z;
}

// 【修正】壁に当たったら true を返すように変更
bool Enemy::MoveSmart(Vector3 target, Dungeon& d) {
    if (eType == E_TRAP) return false;

    Vector3 dir = Vector3Normalize(Vector3Subtract(target, position));
    Vector3 vel = Vector3Scale(dir, speed);
    bool hitWall = false;

    // X軸移動
    if (!d.CheckCollisionRadius(Vector3Add(position, { vel.x, 0, 0 }), radius)) {
        position.x += vel.x;
    }
    else {
        hitWall = true; // X軸で壁に当たった
    }

    // Z軸移動
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, vel.z }), radius)) {
        position.z += vel.z;
    }
    else {
        hitWall = true; // Z軸で壁に当たった
    }

    return hitWall;
}

void Enemy::Update(Player& p, Dungeon& d, EffectManager& fx) {
    if (isDying) {
        std::string key = data.modelName;
        if (!key.empty() && DataManager::loadedModels.count(key) > 0) {
            GameModel& gm = DataManager::loadedModels[key];
            if (gm.animCount > 1) {
                animFrameCounter++;
                if (animFrameCounter >= gm.anims[1].frameCount) {
                    isDead = true;
                }
            }
            else {
                isDead = true;
            }
        }
        else {
            isDead = true;
        }
        return;
    }

    if (hudTimer > 0) hudTimer -= GetFrameTime();
    if (attackTimer > 0) attackTimer -= GetFrameTime();

    animFrameCounter++;

    float dist = Vector3Distance(position, p.position);
    bool canSee = d.HasLineOfSight(position, p.position);

    float effectiveDetect = p.isStealth ? 1.5f : detectRange;
    if (state == STATE_CHASE || state == STATE_ATTACK) {
        if (p.isStealth) effectiveDetect = 3.0f;
    }

    if (dist < attackRange && canSee && dist < effectiveDetect) state = STATE_ATTACK;
    else if (dist < effectiveDetect && canSee) state = STATE_CHASE;
    else state = STATE_PATROL;

    if (state == STATE_CHASE || state == STATE_ATTACK) {
        stuckCount = 0;

        if (eType != E_TRAP) {
            if (dist > attackRange * 0.7f) MoveSmart(p.position, d);
        }

        if (dist < attackRange && attackTimer <= 0) {
            lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));
            Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });

            AudioManager::PlaySE(SE_ENEMY_ATTACK);

            if (eType == E_ARCHER) {
                fx.SpawnProjectile(spawnPos, lastAttackDir, 18.0f, 0, false);
                attackTimer = 1.8f;
            }
            else if (eType == E_MAGE) {
                fx.SpawnProjectile(spawnPos, lastAttackDir, 8.0f, 1, false);
                attackTimer = 2.2f;
            }
            else if (eType == E_TRAP) {
                fx.SpawnProjectile(spawnPos, lastAttackDir, 12.0f, 0, false);
                attackTimer = 2.5f;
            }
            else {
                EffectType effectType = FX_SLASH;
                if (eType == E_SPEAR) effectType = FX_THRUST;
                else if (eType == E_AXE) effectType = FX_SMASH;

                fx.SpawnEffect(spawnPos, lastAttackDir, effectType, GOLD);

                float rawDmg = 10.0f + level * 2;
                float dmg = fmaxf(1.0f, rawDmg - p.defense);
                p.hp -= dmg;

                fx.SpawnDamageText(p.position, (int)dmg);
                fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                attackTimer = 1.5f;
            }
        }
    }
    else {
        // STATE_PATROL
        if (Vector3Distance(position, patrolTarget) < 1.2f) {
            patrolTarget = d.GetRandomFloorPos();
            stuckCount = 0;
        }

        // 【修正】壁に当たったらターゲットを変更する
        bool hitWall = MoveSmart(patrolTarget, d);
        if (hitWall) {
            patrolTarget = d.GetRandomFloorPos();
            stuckCount = 0;
        }

        // スタック対策（動きが小さい場合）
        if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++;
        else stuckCount = 0;

        if (stuckCount > 60) {
            patrolTarget = d.GetRandomFloorPos();
            stuckCount = 0;
        }
    }
    lastPos = position;
}

void Enemy::Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos) {
    int currentAnimIndex = -1;
    bool hasModel = false;

    std::string key = data.modelName;
    if (!key.empty() && DataManager::loadedModels.count(key) > 0) {
        hasModel = true;
        GameModel& gm = DataManager::loadedModels[key];

        gm.model.transform = MatrixIdentity();

        int animIndex = 2; // Idle

        if (isDying) animIndex = 1;
        else if (state == STATE_ATTACK) animIndex = 0;
        else if (state == STATE_CHASE || state == STATE_PATROL) animIndex = 3;

        if (animIndex >= gm.animCount) animIndex = 0;
        currentAnimIndex = animIndex;

        if (gm.animCount > 0) {
            ModelAnimation& anim = gm.anims[animIndex];
            int frame = animFrameCounter;
            if (isDying) {
                if (frame >= anim.frameCount - 1) frame = anim.frameCount - 1;
            }
            else {
                frame = frame % anim.frameCount;
            }
            UpdateModelAnimation(gm.model, anim, frame);
        }

        float rotationAngle = 0.0f;
        Vector3 targetDir = { 0, 0, 1 };

        if (!isDying) {
            if (state == STATE_CHASE || state == STATE_ATTACK) {
                targetDir = Vector3Subtract(playerPos, position);
            }
            else {
                targetDir = Vector3Subtract(patrolTarget, position);
            }

            if (Vector3Length(targetDir) > 0.01f) {
                rotationAngle = atan2f(targetDir.x, targetDir.z) * RAD2DEG;
                // モデルによっては補正が必要
                // rotationAngle -= 90.0f; 
            }
        }

        float scale = 0.01f;

        Matrix matRotX = MatrixRotateX(-90.0f * DEG2RAD);
        Matrix matRotY = MatrixRotateY(rotationAngle * DEG2RAD);

        gm.model.transform = MatrixMultiply(gm.model.transform, matRotX);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotY);

        DrawModel(gm.model, position, scale, WHITE);

        gm.model.transform = MatrixIdentity();
    }
    else {
        Color c = (eType == E_SWORD) ? MAROON :
            (eType == E_SPEAR) ? ORANGE :
            (eType == E_AXE) ? PURPLE :
            (eType == E_ARCHER) ? DARKGREEN :
            (eType == E_MAGE) ? PINK :
            (eType == E_TRAP) ? DARKGRAY : GRAY;

        DrawCube(position, 1, 1.2f, 1, c);
        DrawCubeWires(position, 1, 1.2f, 1, BLACK);
    }

    if (debug) {
        Vector3 headPos = Vector3Add(position, { 0, 2.5f, 0 });
        Color debugColor = (animFrameCounter % 10 < 5) ? GREEN : YELLOW;
        DrawSphere(headPos, 0.2f, debugColor);

        if (hasModel) {
            DrawCubeWires(position, 2.0f, 2.0f, 2.0f, RED);
        }
    }
}