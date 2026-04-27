#include "Enemy.h"
#include "Player.h"
#include "Dungeon.h"
#include "EffectManager.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "UI.h"
#include "rlgl.h"
#include "raymath.h"
#include <math.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>

Matrix GetBoneMatrix(Model model, ModelAnimation anim, int frame, int boneIndex) {
    if (boneIndex < 0 || boneIndex >= model.boneCount) return MatrixIdentity();

    Transform boneTransform = anim.framePoses[frame][boneIndex];

    Matrix mat = MatrixMultiply(
        MatrixMultiply(
            MatrixScale(boneTransform.scale.x, boneTransform.scale.y, boneTransform.scale.z),
            QuaternionToMatrix(boneTransform.rotation)
        ),
        MatrixTranslate(boneTransform.translation.x, boneTransform.translation.y, boneTransform.translation.z)
    );

    return mat;
}

Enemy::Enemy(Vector3 sp, EnemyData d, int fl) {
    position = sp; data = d; eType = (EnemyType)d.type; state = STATE_PATROL; level = fl;

    maxHp = d.hp + (level * 10.0f);
    hp = maxHp;
    speed = d.speed;
    detectRange = d.detect;
    attackRange = d.atkRange;

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

    isBoss = false;
    bossAttackType = 0;
    bossComboStep = 0;
    bossActionTimer = 0.0f;
    bossTargetDir = { 0,0,0 };
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
    if (isDying || isBoss) return;
    Vector3 kb = Vector3Scale(dir, f);
    if (!d.CheckCollisionRadius(Vector3Add(position, { kb.x, 0, 0 }), radius)) position.x += kb.x;
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, kb.z }), radius)) position.z += kb.z;
}

bool Enemy::MoveSmart(Vector3 target, Dungeon& d) {
    if (eType == E_TRAP) return false;

    Vector3 dir = Vector3Normalize(Vector3Subtract(target, position));
    Vector3 vel = Vector3Scale(dir, speed);
    bool hitWall = false;

    if (!d.CheckCollisionRadius(Vector3Add(position, { vel.x, 0, 0 }), radius)) {
        position.x += vel.x;
    }
    else {
        hitWall = true;
    }

    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, vel.z }), radius)) {
        position.z += vel.z;
    }
    else {
        hitWall = true;
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

    if (isBoss) {
        if (dist < effectiveDetect && canSee) {
            state = STATE_ATTACK;
        }
        else {
            state = STATE_PATROL;
        }

        if (state == STATE_ATTACK) {
            stuckCount = 0;
            patrolTarget = p.position;

            if (bossAttackType == 0) {
                if (attackTimer <= 0.0f) {
                    bossAttackType = GetRandomValue(1, 4);
                    bossComboStep = 0;
                    animFrameCounter = 0;
                    bossTargetDir = Vector3Normalize(Vector3Subtract(p.position, position));
                    lastAttackDir = bossTargetDir;

                    if (bossAttackType == 1) bossActionTimer = 0.5f;
                    else if (bossAttackType == 2) bossActionTimer = 0.8f;
                    else if (bossAttackType == 3) bossActionTimer = 1.0f;
                    else if (bossAttackType == 4) bossActionTimer = 1.8f;
                }
                else {
                    if (dist > 3.0f) MoveSmart(p.position, d);
                }
            }
            else {
                if (bossAttackType == 1) {
                    bossActionTimer -= GetFrameTime();
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0;
                        bossComboStep++;
                        AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });

                        fx.SpawnEffect(spawnPos, bossTargetDir, FX_SLASH, GOLD);

                        Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                        if (Vector3Distance(hitCenter, p.position) < 2.5f) {
                            float rawDmg = 10.0f + level * 2;
                            if (bossComboStep == 3) rawDmg *= 1.5f;
                            float dmg = fmaxf(1.0f, rawDmg - p.defense);
                            p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_COMBO"].c_str(), (int)dmg), RED);
                        }

                        if (bossComboStep >= 3) {
                            bossAttackType = 0;
                            attackTimer = 1.5f;
                        }
                        else {
                            bossActionTimer = 0.5f;
                            bossTargetDir = Vector3Normalize(Vector3Subtract(p.position, position));
                            lastAttackDir = bossTargetDir;
                            MoveSmart(Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f)), d);
                        }
                    }
                }
                else if (bossAttackType == 2) {
                    bossActionTimer -= GetFrameTime();
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0;
                        AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        Vector3 spawnPos = Vector3Add(position, { 0, 1.0f, 0 });

                        for (int i = -1; i <= 1; i++) {
                            float angle = i * 20.0f * DEG2RAD;
                            float c = cosf(angle), s = sinf(angle);
                            Vector3 dir = {
                                bossTargetDir.x * c - bossTargetDir.z * s,
                                0.0f,
                                bossTargetDir.x * s + bossTargetDir.z * c
                            };
                            fx.SpawnProjectile(spawnPos, dir, 12.0f, 1, false);
                        }
                        bossAttackType = 0;
                        attackTimer = 1.5f;
                    }
                }
                else if (bossAttackType == 3) {
                    bossActionTimer -= GetFrameTime();
                    if (bossComboStep == 0) {
                        if (bossActionTimer <= 0.0f) {
                            bossComboStep = 1;
                            bossActionTimer = 0.6f;
                            animFrameCounter = 0;
                            AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        }
                        else {
                            if (GetRandomValue(0, 100) < 20) fx.SpawnEffect(Vector3Add(position, { 0,0.5f,0 }), { 0,1,0 }, FX_HIT, ORANGE);
                        }
                    }
                    else if (bossComboStep == 1) {
                        float oldSpeed = speed;
                        speed = speed * 3.5f;
                        bool hitWall = MoveSmart(Vector3Add(position, Vector3Scale(bossTargetDir, 10.0f)), d);
                        speed = oldSpeed;

                        fx.SpawnEffect(Vector3Add(position, { 0,1,0 }), { 0,0,0 }, FX_HIT, Fade(RED, 0.3f));

                        if (Vector3Distance(position, p.position) < radius + p.radius + 0.8f) {
                            float rawDmg = 15.0f + level * 2;
                            float dmg = fmaxf(1.0f, rawDmg - p.defense);
                            p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_DASH"].c_str(), (int)dmg), RED);

                            bossAttackType = 0;
                            attackTimer = 2.0f;
                        }

                        if (hitWall || bossActionTimer <= 0.0f) {
                            bossAttackType = 0;
                            attackTimer = 2.0f;
                        }
                    }
                }
                else if (bossAttackType == 4) {
                    bossActionTimer -= GetFrameTime();
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0;
                        AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        for (int i = 0; i < 12; i++) {
                            float angle = i * 30.0f * DEG2RAD;
                            Vector3 dir = { cosf(angle), 0.0f, sinf(angle) };
                            fx.SpawnEffect(Vector3Add(position, { 0, 0.5f, 0 }), dir, FX_SMASH, PURPLE);
                        }

                        float aoeRadius = 7.0f;
                        if (Vector3Distance(position, p.position) < aoeRadius) {
                            float rawDmg = 20.0f + level * 2;
                            float dmg = fmaxf(1.0f, rawDmg - p.defense);
                            p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_AOE"].c_str(), (int)dmg), RED);
                        }
                        bossAttackType = 0;
                        attackTimer = 2.5f;
                    }
                    else {
                        if (GetRandomValue(0, 100) < 15) {
                            fx.SpawnEffect(Vector3Add(position, { 0,0.5f,0 }), { 0,1,0 }, FX_HIT, MAGENTA);
                        }
                    }
                }
            }
        }
        else {
            if (Vector3Distance(position, patrolTarget) < 1.2f) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            bool hitWall = MoveSmart(patrolTarget, d);
            if (hitWall) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++; else stuckCount = 0;
            if (stuckCount > 60) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
        }
    }
    else
    {
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

                    UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_DMG_TAKEN"].c_str(), data.name.c_str(), (int)dmg), RED);

                    attackTimer = 1.5f;
                }
            }
        }
        else {
            if (Vector3Distance(position, patrolTarget) < 1.2f) {
                patrolTarget = d.GetRandomFloorPos();
                stuckCount = 0;
            }

            bool hitWall = MoveSmart(patrolTarget, d);
            if (hitWall) {
                patrolTarget = d.GetRandomFloorPos();
                stuckCount = 0;
            }

            if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++;
            else stuckCount = 0;

            if (stuckCount > 60) {
                patrolTarget = d.GetRandomFloorPos();
                stuckCount = 0;
            }
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

        int animIndex = 2;

        if (isDying) animIndex = 1;
        else if (state == STATE_ATTACK) {
            if (isBoss) {
                if (bossAttackType == 1) {
                    animIndex = 0;
                }
                else if (bossAttackType == 2 || bossAttackType == 4) {
                    animIndex = 2;
                }
                else if (bossAttackType == 3) {
                    animIndex = (bossComboStep == 1) ? 3 : 2;
                }
                else {
                    if (attackTimer > 1.0f) animIndex = 2;
                    else animIndex = 3;
                }
            }
            else {
                animIndex = 0;
            }
        }
        else if (state == STATE_CHASE || state == STATE_PATROL) {
            animIndex = 3;
        }

        if (animIndex >= gm.animCount) animIndex = 0;
        currentAnimIndex = animIndex;

        int frame = animFrameCounter;
        ModelAnimation anim = gm.anims[currentAnimIndex];

        if (isDying) {
            if (frame >= anim.frameCount - 1) frame = anim.frameCount - 1;
        }
        else {
            frame = frame % anim.frameCount;
        }

        if (gm.animCount > 0) {
            UpdateModelAnimation(gm.model, anim, frame);
        }

        float rotationAngle = 0.0f;
        Vector3 targetDir = { 0, 0, 1 };

        if (!isDying) {
            if (isBoss) {
                if (bossAttackType != 0) targetDir = lastAttackDir;
                else if (state == STATE_ATTACK || state == STATE_CHASE) targetDir = Vector3Subtract(playerPos, position);
                else targetDir = Vector3Subtract(patrolTarget, position);
            }
            else {
                if (state == STATE_CHASE || state == STATE_ATTACK) targetDir = Vector3Subtract(playerPos, position);
                else targetDir = Vector3Subtract(patrolTarget, position);
            }

            if (Vector3Length(targetDir) > 0.01f) {
                rotationAngle = atan2f(targetDir.x, targetDir.z) * RAD2DEG;
            }
        }

        float scale = 0.01f;

        Matrix matRotX = MatrixRotateX(-90.0f * DEG2RAD);
        Matrix matRotY = MatrixRotateY(rotationAngle * DEG2RAD);

        gm.model.transform = MatrixMultiply(gm.model.transform, matRotX);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotY);

        DrawModel(gm.model, position, scale, WHITE);

        int handBoneIndex = -1;
        int weaponBoneIndex = -1;

        for (int i = 0; i < gm.model.boneCount; i++) {
            std::string bName(gm.model.bones[i].name);
            std::string lowerName = bName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            if (lowerName.find("sword") != std::string::npos ||
                lowerName.find("spear") != std::string::npos ||
                lowerName.find("dagger") != std::string::npos) {
                weaponBoneIndex = i;
            }
            if (lowerName.find("hand_r") != std::string::npos ||
                lowerName.find("handright") != std::string::npos ||
                lowerName.find("handaright") != std::string::npos) {
                handBoneIndex = i;
            }
        }

        int targetBoneIndex = (weaponBoneIndex != -1) ? weaponBoneIndex : handBoneIndex;

        if (targetBoneIndex != -1) {
            Matrix matScale = MatrixScale(scale, scale, scale);
            Matrix matTrans = MatrixTranslate(position.x, position.y, position.z);
            Matrix modelBaseTransform = MatrixMultiply(MatrixMultiply(gm.model.transform, matScale), matTrans);

            Matrix boneMatrix = GetBoneMatrix(gm.model, anim, frame, targetBoneIndex);
            Matrix boneWorldTransform = MatrixMultiply(boneMatrix, modelBaseTransform);

            if (!data.weaponModelName.empty() && DataManager::loadedModels.count(data.weaponModelName) > 0) {
                GameModel& wGm = DataManager::loadedModels[data.weaponModelName];
                wGm.model.transform = boneWorldTransform;

                if (wGm.animCount > 0) {
                    UpdateModelAnimation(wGm.model, wGm.anims[0], frame % wGm.anims[0].frameCount);
                }
                DrawModel(wGm.model, { 0,0,0 }, 1.0f, WHITE);
                wGm.model.transform = MatrixIdentity();
            }
            else if (debug) {
                Vector3 boneWorldPos = { boneWorldTransform.m12, boneWorldTransform.m13, boneWorldTransform.m14 };
                DrawCube(boneWorldPos, 0.2f, 0.2f, 0.2f, GREEN);
                DrawCubeWires(boneWorldPos, 0.2f, 0.2f, 0.2f, LIME);

                Vector3 boneForward = { boneWorldTransform.m8, boneWorldTransform.m9, boneWorldTransform.m10 };
                Vector3 tip = Vector3Add(boneWorldPos, Vector3Scale(Vector3Normalize(boneForward), 0.8f));
                DrawLine3D(boneWorldPos, tip, RED);
            }
        }
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
        if (hasModel) DrawCubeWires(position, 2.0f, 2.0f, 2.0f, RED);
    }

    if (!isDying && isBoss && bossAttackType != 0) {
        float maxTime = 1.0f;
        if (bossAttackType == 1) maxTime = 0.5f;
        else if (bossAttackType == 2) maxTime = 0.8f;
        else if (bossAttackType == 3) maxTime = 1.0f;
        else if (bossAttackType == 4) maxTime = 1.8f;

        bool showTelegraph = true;
        if (bossAttackType == 3 && bossComboStep == 1) showTelegraph = false;

        if (showTelegraph) {
            float progress = 1.0f - (bossActionTimer / maxTime);
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;

            Color warnColor = Fade(RED, progress * 0.4f);
            Color warnLineColor = Fade(MAROON, progress * 0.8f);

            if (bossAttackType == 1) {
                Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                DrawCylinder({ hitCenter.x, 0.05f, hitCenter.z }, 2.5f, 2.5f, 0.05f, 32, warnColor);
                DrawCylinderWires({ hitCenter.x, 0.05f, hitCenter.z }, 2.5f, 2.5f, 0.05f, 32, warnLineColor);
            }
            else if (bossAttackType == 2) {
                for (int i = -1; i <= 1; i++) {
                    float angle = i * 20.0f * DEG2RAD;
                    float c = cosf(angle), s = sinf(angle);
                    Vector3 dir = {
                        bossTargetDir.x * c - bossTargetDir.z * s,
                        0.0f,
                        bossTargetDir.x * s + bossTargetDir.z * c
                    };

                    float len = 20.0f;
                    float w = 1.5f;
                    float rot = atan2f(dir.x, dir.z) * RAD2DEG;
                    rlPushMatrix();
                    rlTranslatef(position.x, 0.05f, position.z);
                    rlRotatef(rot, 0.0f, 1.0f, 0.0f);
                    DrawCube({ 0.0f, 0.0f, len / 2.0f }, w, 0.05f, len, warnColor);
                    DrawCubeWires({ 0.0f, 0.0f, len / 2.0f }, w, 0.05f, len, warnLineColor);
                    rlPopMatrix();
                }
            }
            else if (bossAttackType == 3) {
                float len = 10.0f + 2.75f;
                float w = 5.5f;
                float rot = atan2f(bossTargetDir.x, bossTargetDir.z) * RAD2DEG;
                rlPushMatrix();
                rlTranslatef(position.x, 0.05f, position.z);
                rlRotatef(rot, 0.0f, 1.0f, 0.0f);
                DrawCube({ 0.0f, 0.0f, len / 2.0f }, w, 0.05f, len, warnColor);
                DrawCubeWires({ 0.0f, 0.0f, len / 2.0f }, w, 0.05f, len, warnLineColor);
                rlPopMatrix();
            }
            else if (bossAttackType == 4) {
                DrawCylinder({ position.x, 0.05f, position.z }, 7.0f, 7.0f, 0.05f, 32, warnColor);
                DrawCylinderWires({ position.x, 0.05f, position.z }, 7.0f, 7.0f, 0.05f, 32, warnLineColor);
            }
        }
    }
}