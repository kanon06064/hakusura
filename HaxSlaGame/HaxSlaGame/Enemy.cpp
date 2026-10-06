#include "Enemy.h"
#include "Player.h"
#include "Dungeon.h"
#include "EffectManager.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "UI.h"
#include "raymath.h"
#include <math.h>
#include <string>
#include <iostream>
#include <algorithm>
#include <cctype>

// 敵キャラクターのボーン行列を取得する関数
Matrix GetBoneMatrix(Model model, ModelAnimation anim, int frame, int boneIndex) {
    if (boneIndex < 0 || boneIndex >= model.boneCount) return MatrixIdentity();
    Transform boneTransform = anim.framePoses[frame][boneIndex];
    Matrix mat = MatrixMultiply(
        MatrixMultiply(MatrixScale(boneTransform.scale.x, boneTransform.scale.y, boneTransform.scale.z), QuaternionToMatrix(boneTransform.rotation)),
        MatrixTranslate(boneTransform.translation.x, boneTransform.translation.y, boneTransform.translation.z)
    );
    return mat;
}

Enemy::Enemy(Vector3 sp, EnemyData d, int fl) {
    position = sp;
    data = d;
    eType = (EnemyType)d.type;
    state = STATE_PATROL;
    level = fl;

    maxHp = d.hp + (level * 10.0f);
    hp = maxHp;
    speed = d.speed;
    detectRange = d.detect;
    attackRange = d.atkRange;

    int levelBonus = (int)((float)d.exp * 0.1f * (float)level) + level;
    expValue = d.exp + levelBonus;

    radius = 0.45f;
    attackTimer = 0.0f;
    attackAnimTimer = 0.0f;

    // 攻撃予兆（チャージ）タイマーの初期化
    isChargingAttack = false;
    attackChargeTimer = 0.0f;
    attackChargeMax = 0.6f;

    hudTimer = 0.0f;
    lastAttackDir = { 1.0f, 0.0f, 0.0f };
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
    bossTargetDir = { 0.0f, 0.0f, 0.0f };
}

void Enemy::StartDeath() {
    if (isDying) return;
    isDying = true; animFrameCounter = 0;
    std::string key = data.modelName;
    if (key.empty() || DataManager::loadedModels.count(key) == 0) { isDead = true; }
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

    if (!d.CheckCollisionRadius(Vector3Add(position, { vel.x, 0, 0 }), radius)) { position.x += vel.x; }
    else { hitWall = true; }
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, vel.z }), radius)) { position.z += vel.z; }
    else { hitWall = true; }
    return hitWall;
}

void Enemy::Update(Player& p, Dungeon& d, EffectManager& fx) {
    if (isDying) {
        std::string key = data.modelName;
        if (!key.empty() && DataManager::loadedModels.count(key) > 0) {
            GameModel& gm = DataManager::loadedModels[key];
            if (gm.animCount > 1) {
                animFrameCounter++;
                if (animFrameCounter >= gm.anims[1].frameCount) { isDead = true; }
            }
            else { isDead = true; }
        }
        else { isDead = true; }
        return;
    }

    float dt = GetFrameTime();
    if (hudTimer > 0) hudTimer -= dt;
    if (attackTimer > 0) attackTimer -= dt;
    if (attackAnimTimer > 0) attackAnimTimer -= dt;
    animFrameCounter++;

    float dist = Vector3Distance(position, p.position);
    bool canSee = d.HasLineOfSight(position, p.position);

    float effectiveDetect = p.isStealth ? 1.5f : detectRange;
    if (state == STATE_CHASE || state == STATE_ATTACK) { if (p.isStealth) effectiveDetect = 3.0f; }

    // =========================================================================
    // ボスAI
    // =========================================================================
    if (isBoss) {
        if (dist < effectiveDetect && canSee) { state = STATE_ATTACK; }
        else { state = STATE_PATROL; }

        if (state == STATE_ATTACK) {
            stuckCount = 0; patrolTarget = p.position;

            if (bossAttackType == 0) {
                if (attackTimer <= 0.0f) {
                    bossAttackType = GetRandomValue(1, 4); bossComboStep = 0; animFrameCounter = 0;
                    bossTargetDir = Vector3Normalize(Vector3Subtract(p.position, position));
                    lastAttackDir = bossTargetDir;

                    if (bossAttackType == 1) bossActionTimer = 0.55f;
                    else if (bossAttackType == 2) bossActionTimer = 0.9f;
                    else if (bossAttackType == 3) bossActionTimer = 1.0f;
                    else if (bossAttackType == 4) bossActionTimer = 2.0f;
                }
                else {
                    if (dist > 3.0f) MoveSmart(p.position, d);
                }
            }
            else {
                if (bossAttackType == 1) {
                    bossActionTimer -= dt;
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0; bossComboStep++; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });
                        Color atkCol = (data.element != ELEM_NONE) ? Player::GetElementColor(data.element) : GOLD;
                        fx.SpawnEffect(spawnPos, bossTargetDir, FX_SLASH, atkCol);

                        Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                        if (Vector3Distance(hitCenter, p.position) < 2.5f) {
                            float rawDmg = 10.0f + level * 2; if (bossComboStep == 3) rawDmg *= 1.5f;
                            float defDmg = fmaxf(1.0f, rawDmg - p.defense);

                            float resist = p.GetPlayerElementResistance(data.element);
                            float finalDmg = fmaxf(1.0f, defDmg * (1.0f - resist));
                            p.hp -= finalDmg;

                            fx.SpawnDamageText(p.position, (int)finalDmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                            if (resist > 0.05f) {
                                std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                            }
                            else {
                                UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_COMBO"].c_str(), (int)finalDmg), RED);
                            }

                            if (bossComboStep >= 3) fx.ShakeScreen(0.3f, 0.6f);
                            else fx.ShakeScreen(0.15f, 0.35f);
                            fx.TriggerDamageFlash(0.25f);
                        }

                        if (bossComboStep >= 3) { bossAttackType = 0; attackTimer = 1.5f; }
                        else {
                            bossActionTimer = 0.55f;
                            bossTargetDir = Vector3Normalize(Vector3Subtract(p.position, position));
                            lastAttackDir = bossTargetDir;
                            MoveSmart(Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f)), d);
                        }
                    }
                }
                else if (bossAttackType == 2) {
                    bossActionTimer -= dt;
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        Vector3 spawnPos = Vector3Add(position, { 0, 1.0f, 0 });
                        for (int i = -1; i <= 1; i++) {
                            float angle = i * 20.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                            Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                            fx.SpawnProjectile(spawnPos, dir, 12.0f, 1, false);
                        }
                        bossAttackType = 0; attackTimer = 1.5f;
                    }
                }
                else if (bossAttackType == 3) {
                    bossActionTimer -= dt;
                    if (bossComboStep == 0) {
                        if (bossActionTimer <= 0.0f) { bossComboStep = 1; bossActionTimer = 0.6f; animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK); }
                        else { if (GetRandomValue(0, 100) < 20) fx.SpawnEffect(Vector3Add(position, { 0,0.5f,0 }), { 0,1,0 }, FX_HIT, ORANGE); }
                    }
                    else if (bossComboStep == 1) {
                        float oldSpeed = speed; speed = speed * 3.5f;
                        bool hitWall = MoveSmart(Vector3Add(position, Vector3Scale(bossTargetDir, 10.0f)), d);
                        speed = oldSpeed;

                        fx.SpawnEffect(Vector3Add(position, { 0,1,0 }), { 0,0,0 }, FX_HIT, Fade(RED, 0.3f));

                        if (Vector3Distance(position, p.position) < radius + p.radius + 0.8f) {
                            float rawDmg = 15.0f + level * 2;
                            float defDmg = fmaxf(1.0f, rawDmg - p.defense);

                            float resist = p.GetPlayerElementResistance(data.element);
                            float finalDmg = fmaxf(1.0f, defDmg * (1.0f - resist));
                            p.hp -= finalDmg;

                            fx.SpawnDamageText(p.position, (int)finalDmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                            if (resist > 0.05f) {
                                std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                            }
                            else {
                                UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_DASH"].c_str(), (int)finalDmg), RED);
                            }

                            fx.ShakeScreen(0.35f, 0.7f);
                            fx.TriggerDamageFlash(0.35f);
                            bossAttackType = 0; attackTimer = 2.0f;
                        }
                        if (hitWall || bossActionTimer <= 0.0f) { bossAttackType = 0; attackTimer = 2.0f; }
                    }
                }
                else if (bossAttackType == 4) {
                    bossActionTimer -= dt;
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        for (int i = 0; i < 12; i++) {
                            float angle = i * 30.0f * DEG2RAD; Vector3 dir = { cosf(angle), 0.0f, sinf(angle) };
                            fx.SpawnEffect(Vector3Add(position, { 0, 0.5f, 0 }), dir, FX_SMASH, PURPLE);
                        }
                        fx.ShakeScreen(0.4f, 0.7f);

                        float aoeRadius = 7.0f;
                        if (Vector3Distance(position, p.position) < aoeRadius) {
                            float rawDmg = 20.0f + level * 2;
                            float defDmg = fmaxf(1.0f, rawDmg - p.defense);

                            float resist = p.GetPlayerElementResistance(data.element);
                            float finalDmg = fmaxf(1.0f, defDmg * (1.0f - resist));
                            p.hp -= finalDmg;

                            fx.SpawnDamageText(p.position, (int)finalDmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                            if (resist > 0.05f) {
                                std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                            }
                            else {
                                UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_AOE"].c_str(), (int)finalDmg), RED);
                            }

                            fx.TriggerDamageFlash(0.35f);
                        }
                        bossAttackType = 0; attackTimer = 2.5f;
                    }
                    else { if (GetRandomValue(0, 100) < 15) fx.SpawnEffect(Vector3Add(position, { 0,0.5f,0 }), { 0,1,0 }, FX_HIT, MAGENTA); }
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
    // =========================================================================
    // 通常敵AI
    // =========================================================================
    else {
        if (dist < attackRange && canSee && dist < effectiveDetect) state = STATE_ATTACK;
        else if (dist < effectiveDetect && canSee) state = STATE_CHASE;
        else state = STATE_PATROL;

        if (state == STATE_CHASE || state == STATE_ATTACK) {
            stuckCount = 0;
            if (!isChargingAttack && eType != E_TRAP) {
                if (dist > attackRange * 0.7f) MoveSmart(p.position, d);
            }

            if (dist < attackRange && attackTimer <= 0.0f && !isChargingAttack) {
                isChargingAttack = true;
                lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));

                if (eType == E_AXE) attackChargeMax = 0.85f;
                else if (eType == E_SPEAR) attackChargeMax = 0.65f;
                else attackChargeMax = 0.5f;

                attackChargeTimer = attackChargeMax;
            }

            if (isChargingAttack) {
                attackChargeTimer -= dt;
                lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));

                if (attackChargeTimer <= 0.0f) {
                    isChargingAttack = false;
                    attackAnimTimer = 0.5f;
                    animFrameCounter = 0;

                    Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });
                    AudioManager::PlaySE(SE_ENEMY_ATTACK);

                    if (eType == E_ARCHER) { fx.SpawnProjectile(spawnPos, lastAttackDir, 18.0f, 0, false); attackTimer = 1.8f; }
                    else if (eType == E_MAGE) { fx.SpawnProjectile(spawnPos, lastAttackDir, 8.0f, 1, false); attackTimer = 2.2f; }
                    else if (eType == E_TRAP) { fx.SpawnProjectile(spawnPos, lastAttackDir, 12.0f, 0, false); attackTimer = 2.5f; }
                    else {
                        EffectType effectType = FX_SLASH;
                        if (eType == E_SPEAR) effectType = FX_THRUST;
                        else if (eType == E_AXE) effectType = FX_SMASH;

                        Color atkCol = (data.element != ELEM_NONE) ? Player::GetElementColor(data.element) : GOLD;
                        fx.SpawnEffect(spawnPos, lastAttackDir, effectType, atkCol);

                        float aoeRadius = attackRange * 0.9f;
                        Vector3 hitCenter = Vector3Add(position, Vector3Scale(lastAttackDir, attackRange * 0.5f));

                        if (Vector3Distance(hitCenter, p.position) < aoeRadius) {
                            float rawDmg = 10.0f + level * 2;
                            float defDmg = fmaxf(1.0f, rawDmg - p.defense);

                            float resist = p.GetPlayerElementResistance(data.element);
                            float finalDmg = fmaxf(1.0f, defDmg * (1.0f - resist));
                            p.hp -= finalDmg;

                            fx.SpawnDamageText(p.position, (int)finalDmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                            // ★ 文字化け防止：安全な被弾ログ出力
                            if (resist > 0.05f) {
                                std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                            }
                            else {
                                std::string takenFmt = DataManager::uiStrings.count("LOG_DMG_TAKEN") ? DataManager::uiStrings["LOG_DMG_TAKEN"] : "%s HIT: %d DMG!";
                                UI::AddSystemLog(TextFormat(takenFmt.c_str(), data.name.c_str(), (int)finalDmg), RED);
                            }

                            if (eType == E_AXE) fx.ShakeScreen(0.2f, 0.45f);
                            else fx.ShakeScreen(0.15f, 0.3f);
                            fx.TriggerDamageFlash(0.22f);
                        }

                        attackTimer = 1.5f;
                    }
                }
            }
        }
        else {
            isChargingAttack = false;
            if (Vector3Distance(position, patrolTarget) < 1.2f) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            bool hitWall = MoveSmart(patrolTarget, d);
            if (hitWall) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++; else stuckCount = 0;
            if (stuckCount > 60) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
        }
    }
    lastPos = position;
}

void Enemy::Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos) {
    bool hasModel = false;

    std::string key = data.modelName;
    if (!key.empty() && DataManager::loadedModels.count(key) > 0) {
        hasModel = true;
        GameModel& gm = DataManager::loadedModels[key];
        gm.model.transform = MatrixIdentity();

        // --- アニメーション番号の決定 ---
        int animIndex = 2; // デフォルトは待機(Idle)
        float comboProgress = 1.0f - (bossActionTimer / 0.55f);

        if (isDying) {
            animIndex = 1;
        }
        else if (isBoss) {
            if (bossAttackType == 1) {
                // 0.70まで構えて力を溜め、残り0.30で一気に振り下ろす！
                animIndex = (comboProgress < 0.70f) ? 2 : 0;
            }
            else if (bossAttackType == 2 || bossAttackType == 4) animIndex = 2;
            else if (bossAttackType == 3) animIndex = (bossComboStep == 1) ? 3 : 2;
            else {
                // bossAttackType == 0（移動アプローチフェーズ）
                float distToPlayer = Vector3Distance(position, playerPos);
                if (state == STATE_PATROL) animIndex = 3;
                else if (state == STATE_ATTACK && distToPlayer > 3.0f) animIndex = 3;
                else animIndex = 2;
            }
        }
        else {
            if (attackAnimTimer > 0.0f) animIndex = 0;
            else if (isChargingAttack) animIndex = 2;
            else if (state == STATE_CHASE || state == STATE_PATROL) animIndex = 3;
            else animIndex = 2;
        }

        if (animIndex >= gm.animCount) animIndex = 0;
        int currentAnimIndex = animIndex;

        // --- フレーム番号の決定（時間と完全同期） ---
        ModelAnimation anim = gm.anims[currentAnimIndex];
        int frame = 0;

        if (isDying) {
            frame = (animFrameCounter >= anim.frameCount - 1) ? anim.frameCount - 1 : animFrameCounter;
        }
        // ボスコンボ斬撃：0.70から振り下ろしを開始しタイマー0で振り下ろし完了
        else if (isBoss && bossAttackType == 1 && comboProgress >= 0.70f) {
            float swingProgress = (comboProgress - 0.70f) / 0.30f;
            if (swingProgress > 1.0f) swingProgress = 1.0f;
            frame = (int)(swingProgress * (float)(anim.frameCount - 1));
        }
        else if (!isBoss && attackAnimTimer > 0.0f) {
            float attackProgress = 1.0f - (attackAnimTimer / 0.5f);
            if (attackProgress < 0.0f) attackProgress = 0.0f;
            if (attackProgress > 1.0f) attackProgress = 1.0f;
            frame = (int)(attackProgress * (float)(anim.frameCount - 1));
        }
        else {
            frame = (anim.frameCount > 0) ? (animFrameCounter % anim.frameCount) : 0;
        }

        if (gm.animCount > 0 && gm.anims != nullptr) {
            UpdateModelAnimation(gm.model, anim, frame);
        }

        // --- 向き（回転角度）の計算 ---
        float rotationAngle = 0.0f;
        Vector3 targetDir = { 0, 0, 1 };

        if (!isDying) {
            if (isBoss) {
                if (bossAttackType != 0) targetDir = lastAttackDir;
                else if (state == STATE_ATTACK || state == STATE_CHASE) targetDir = Vector3Subtract(playerPos, position);
                else targetDir = Vector3Subtract(patrolTarget, position);
            }
            else {
                if (isChargingAttack) targetDir = lastAttackDir;
                else if (state == STATE_CHASE || state == STATE_ATTACK) targetDir = Vector3Subtract(playerPos, position);
                else targetDir = Vector3Subtract(patrolTarget, position);
            }

            targetDir.y = 0.0f; // 水平化
            if (Vector3Length(targetDir) > 0.01f) {
                rotationAngle = atan2f(targetDir.x, targetDir.z) * RAD2DEG;
            }
        }

        float scale = 0.01f;
        Vector3 drawPos = { position.x, position.y - 0.4f, position.z };

        Matrix matRotX = MatrixRotateX(-90.0f * DEG2RAD);
        Matrix matRotY = MatrixRotateY(rotationAngle * DEG2RAD);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotX);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotY);

        DrawModel(gm.model, drawPos, scale, WHITE);

        // 武器ボーン追従
        int handBoneIndex = -1; int weaponBoneIndex = -1;
        for (int i = 0; i < gm.model.boneCount; i++) {
            std::string bName(gm.model.bones[i].name); std::string lowerName = bName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            if (lowerName.find("sword") != std::string::npos || lowerName.find("spear") != std::string::npos || lowerName.find("dagger") != std::string::npos) { weaponBoneIndex = i; }
            if (lowerName.find("hand_r") != std::string::npos || lowerName.find("handright") != std::string::npos || lowerName.find("handaright") != std::string::npos) { handBoneIndex = i; }
        }

        int targetBoneIndex = (weaponBoneIndex != -1) ? weaponBoneIndex : handBoneIndex;
        if (targetBoneIndex != -1) {
            Matrix matScale = MatrixScale(scale, scale, scale);
            Matrix matTrans = MatrixTranslate(drawPos.x, drawPos.y, drawPos.z);
            Matrix modelBaseTransform = MatrixMultiply(MatrixMultiply(gm.model.transform, matScale), matTrans);

            Matrix boneMatrix = GetBoneMatrix(gm.model, anim, frame, targetBoneIndex);
            Matrix boneWorldTransform = MatrixMultiply(boneMatrix, modelBaseTransform);

            if (!data.weaponModelName.empty() && DataManager::loadedModels.count(data.weaponModelName) > 0) {
                GameModel& wGm = DataManager::loadedModels[data.weaponModelName];
                wGm.model.transform = boneWorldTransform;

                if (wGm.animCount > 0 && wGm.anims != nullptr) {
                    UpdateModelAnimation(wGm.model, wGm.anims[0], frame % wGm.anims[0].frameCount);
                }
                DrawModel(wGm.model, { 0,0,0 }, 1.0f, WHITE);
                wGm.model.transform = MatrixIdentity();
            }
        }
        gm.model.transform = MatrixIdentity();
    }
    else {
        Color c = (eType == E_SWORD) ? MAROON : (eType == E_SPEAR) ? ORANGE : (eType == E_AXE) ? PURPLE : (eType == E_ARCHER) ? DARKGREEN : (eType == E_MAGE) ? PINK : (eType == E_TRAP) ? DARKGRAY : GRAY;
        Vector3 drawPos = { position.x, position.y - 0.4f, position.z };
        DrawCube(drawPos, 1, 1.2f, 1, c); DrawCubeWires(drawPos, 1, 1.2f, 1, BLACK);
    }

    if (debug) {
        Vector3 headPos = Vector3Add(position, { 0, 2.5f, 0 });
        Color debugColor = (animFrameCounter % 10 < 5) ? GREEN : YELLOW;
        DrawSphere(headPos, 0.2f, debugColor);
        if (hasModel) DrawCubeWires(position, 2.0f, 2.0f, 2.0f, RED);
    }

    // =========================================================================
    // 通常敵のAoE予兆表示
    // =========================================================================
    if (!isDying && !isBoss && isChargingAttack && (eType == E_SWORD || eType == E_AXE || eType == E_SPEAR)) {
        float progress = 1.0f - (attackChargeTimer / attackChargeMax);
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        float aoeRadius = attackRange * 0.9f;
        Vector3 hitCenter = Vector3Add(position, Vector3Scale(lastAttackDir, attackRange * 0.5f));
        hitCenter.y = 0.12f;

        DrawCylinderWires(hitCenter, aoeRadius, aoeRadius, 0.04f, 32, Fade(RED, 0.95f));
        float fillAlpha = 0.10f + (progress * 0.55f);
        DrawCylinder(hitCenter, aoeRadius, aoeRadius, 0.03f, 32, Fade(RED, fillAlpha));
        float gaugeRadius = aoeRadius * progress;
        DrawCylinder(hitCenter, gaugeRadius, gaugeRadius, 0.05f, 32, Fade(ORANGE, 0.5f + progress * 0.4f));
    }

    // =========================================================================
    // ボスのAoE予兆表示
    // =========================================================================
    if (!isDying && isBoss && bossAttackType != 0) {
        float maxTime = 1.0f;
        if (bossAttackType == 1) maxTime = 0.55f;
        else if (bossAttackType == 2) maxTime = 0.9f;
        else if (bossAttackType == 3) maxTime = 1.0f;
        else if (bossAttackType == 4) maxTime = 2.0f;

        bool showTelegraph = true;
        if (bossAttackType == 3 && bossComboStep == 1) showTelegraph = false;

        if (showTelegraph) {
            float progress = 1.0f - (bossActionTimer / maxTime);
            if (progress < 0.0f) progress = 0.0f;
            if (progress > 1.0f) progress = 1.0f;

            Color outlineColor = Fade(RED, 0.95f);
            float baseAlpha = 0.12f + (progress * 0.45f);
            if (progress > 0.8f && (animFrameCounter % 6 < 3)) baseAlpha += 0.2f;
            Color fillColor = Fade(PURPLE, baseAlpha);
            Color chargeColor = Fade(RED, 0.45f + progress * 0.45f);

            // 1. ボスの巨大全方位AoE（bossAttackType == 4）
            if (bossAttackType == 4) {
                float totalRadius = 7.0f;
                Vector3 center = { position.x, 0.12f, position.z };

                DrawCylinder(center, totalRadius, totalRadius, 0.04f, 36, fillColor);
                DrawCylinder(center, totalRadius * progress, totalRadius * progress, 0.05f, 36, chargeColor);
                DrawCylinderWires(center, totalRadius, totalRadius, 0.04f, 36, outlineColor);
                DrawCylinderWires(center, totalRadius + 0.05f, totalRadius + 0.05f, 0.04f, 36, Fade(RED, 0.6f));
            }
            // 2. ボスのコンボ斬撃（bossAttackType == 1）
            else if (bossAttackType == 1) {
                float comboRadius = 2.5f;
                Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                hitCenter.y = 0.12f;

                DrawCylinder(hitCenter, comboRadius, comboRadius, 0.04f, 32, Fade(RED, baseAlpha));
                DrawCylinder(hitCenter, comboRadius * progress, comboRadius * progress, 0.05f, 32, Fade(ORANGE, 0.7f));
                DrawCylinderWires(hitCenter, comboRadius, comboRadius, 0.04f, 32, outlineColor);
            }
            // 3. ボスの3方向弾幕レーン（bossAttackType == 2）
            else if (bossAttackType == 2) {
                for (int i = -1; i <= 1; i++) {
                    float angle = i * 20.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                    Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                    float totalLen = 20.0f; float w = 1.5f;
                    Vector3 side = { -dir.z, 0.0f, dir.x };

                    Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                    Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
                    Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
                    Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                    p1.y = p2.y = p3.y = p4.y = 0.12f;

                    DrawTriangle3D(p1, p4, p3, fillColor); DrawTriangle3D(p1, p3, p2, fillColor);
                    DrawTriangle3D(p3, p4, p1, fillColor); DrawTriangle3D(p2, p3, p1, fillColor);

                    float chargeLen = totalLen * progress;
                    Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
                    Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
                    cp2.y = cp3.y = 0.13f;
                    DrawTriangle3D(p1, p4, cp3, chargeColor); DrawTriangle3D(p1, cp3, cp2, chargeColor);
                    DrawTriangle3D(cp3, p4, p1, chargeColor); DrawTriangle3D(cp2, cp3, p1, chargeColor);

                    DrawLine3D(p1, p2, outlineColor); DrawLine3D(p2, p3, outlineColor);
                    DrawLine3D(p3, p4, outlineColor); DrawLine3D(p4, p1, outlineColor);
                }
            }
            // 4. ボスの突進矩形レーン（bossAttackType == 3）
            else if (bossAttackType == 3) {
                float totalLen = 12.75f; float w = 5.5f;
                Vector3 dir = Vector3Normalize(bossTargetDir);
                Vector3 side = { -dir.z, 0.0f, dir.x };

                Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
                Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
                Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                p1.y = p2.y = p3.y = p4.y = 0.12f;

                DrawTriangle3D(p1, p4, p3, fillColor); DrawTriangle3D(p1, p3, p2, fillColor);
                DrawTriangle3D(p3, p4, p1, fillColor); DrawTriangle3D(p2, p3, p1, fillColor);

                float chargeLen = totalLen * progress;
                Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
                Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
                cp2.y = cp3.y = 0.13f;
                DrawTriangle3D(p1, p4, cp3, chargeColor); DrawTriangle3D(p1, cp3, cp2, chargeColor);
                DrawTriangle3D(cp3, p4, p1, chargeColor); DrawTriangle3D(cp2, cp3, p1, chargeColor);

                DrawLine3D(p1, p2, outlineColor); DrawLine3D(p2, p3, outlineColor);
                DrawLine3D(p3, p4, outlineColor); DrawLine3D(p4, p1, outlineColor);
            }
        }
    }
}