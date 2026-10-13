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

Matrix GetBoneMatrix(Model model, ModelAnimation anim, int frame, int boneIndex) {
    if (boneIndex < 0 || boneIndex >= model.boneCount) return MatrixIdentity();
    Transform boneTransform = anim.framePoses[frame][boneIndex];
    Matrix mat = MatrixMultiply(
        MatrixMultiply(MatrixScale(boneTransform.scale.x, boneTransform.scale.y, boneTransform.scale.z), QuaternionToMatrix(boneTransform.rotation)),
        MatrixTranslate(boneTransform.translation.x, boneTransform.translation.y, boneTransform.translation.z)
    );
    return mat;
}

bool Enemy::IsPlant() const {
    if (eType == E_PLANT) return true;
    std::string n = data.name;
    std::string m = data.modelName;
    for (auto& c : n) c = (char)tolower(c);
    for (auto& c : m) c = (char)tolower(c);

    if (n.find("plant") != std::string::npos || n.find("tree") != std::string::npos ||
        n.find("flower") != std::string::npos || n.find("プラント") != std::string::npos ||
        n.find("魔界樹") != std::string::npos || n.find("樹") != std::string::npos) {
        return true;
    }
    if (m.find("plant") != std::string::npos || m.find("tree") != std::string::npos) {
        return true;
    }
    return false;
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

    if (IsPlant()) {
        speed = 0.0f;
        detectRange = fmaxf(detectRange, 14.0f);
        attackRange = fmaxf(attackRange, 10.0f);
    }

    int levelBonus = (int)((float)d.exp * 0.1f * (float)level) + level;
    expValue = d.exp + levelBonus;

    radius = 0.45f;
    attackTimer = 0.0f;
    attackAnimTimer = 0.0f;

    isChargingAttack = false;
    attackChargeTimer = 0.0f;
    attackChargeMax = 0.6f;
    normalAttackPattern = 0;

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

    delayedHp = maxHp;
    delayedHpTimer = 0.0f;
}

void Enemy::StartDeath() {
    if (isDying) return;
    isDying = true; animFrameCounter = 0;
    std::string key = data.modelName;
    if (key.empty() || DataManager::loadedModels.count(key) == 0) { isDead = true; }
}

void Enemy::ApplyKnockback(Vector3 dir, float f, Dungeon& d) {
    if (isDying || isBoss || IsPlant()) return;
    Vector3 kb = Vector3Scale(dir, f);
    if (!d.CheckCollisionRadius(Vector3Add(position, { kb.x, 0, 0 }), radius)) position.x += kb.x;
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, kb.z }), radius)) position.z += kb.z;
}

bool Enemy::MoveSmart(Vector3 target, Dungeon& d) {
    if (eType == E_TRAP || IsPlant()) return false;

    Vector3 dir = Vector3Normalize(Vector3Subtract(target, position));
    float currentSpeed = speed * status.GetSpeedMultiplier();
    Vector3 vel = Vector3Scale(dir, currentSpeed);
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

    if (isBoss) {
        if (delayedHpTimer > 0.0f) {
            delayedHpTimer -= dt;
        }
        else {
            if (delayedHp > hp) {
                float dropSpeed = maxHp * 0.45f * dt;
                delayedHp -= dropSpeed;
                if (delayedHp < hp) delayedHp = hp;
            }
        }
    }

    status.Update(dt, hp, fx, position);

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

                    if (IsPlant()) {
                        if (bossAttackType == 1) bossActionTimer = 0.75f;
                        else if (bossAttackType == 2) bossActionTimer = 1.1f;
                        else if (bossAttackType == 3) bossActionTimer = 0.9f;
                        else if (bossAttackType == 4) bossActionTimer = 1.8f;
                    }
                    else {
                        if (bossAttackType == 1) bossActionTimer = 0.55f;
                        else if (bossAttackType == 2) bossActionTimer = 0.9f;
                        else if (bossAttackType == 3) bossActionTimer = 1.0f;
                        else if (bossAttackType == 4) bossActionTimer = 2.0f;
                    }
                }
                else {
                    if (!IsPlant() && dist > 3.0f) MoveSmart(p.position, d);
                }
            }
            else {
                // 植物ボスの攻撃処理
                if (IsPlant()) {
                    bossActionTimer -= dt;

                    // 1. 5方向拡散毒弾幕（威力スケール化: 24.0f + level * 2.5f）
                    if (bossAttackType == 1) {
                        if (bossActionTimer <= 0.0f) {
                            animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                            Vector3 spawnPos = Vector3Add(position, { 0, 0.75f, 0 });
                            float bDmg = 24.0f + level * 2.5f;

                            for (int i = -2; i <= 2; i++) {
                                float angle = i * 16.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                                Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                                fx.SpawnProjectile(spawnPos, dir, 12.0f, 2, false, bDmg, ELEM_ROT);
                            }
                            bossAttackType = 0; attackTimer = 1.6f;
                        }
                    }
                    // 2. 全方位360度スパイラル毒胞子弾幕（威力スケール化: 18.0f + level * 2.0f）
                    else if (bossAttackType == 2) {
                        if (bossActionTimer <= 0.0f) {
                            animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                            Vector3 spawnPos = Vector3Add(position, { 0, 0.75f, 0 });
                            float bDmg = 18.0f + level * 2.0f;

                            for (int i = 0; i < 16; i++) {
                                float angle = i * 22.5f * DEG2RAD;
                                Vector3 dir = { cosf(angle), 0.0f, sinf(angle) };
                                fx.SpawnProjectile(spawnPos, dir, 9.5f, 2, false, bDmg, ELEM_ROT);
                            }
                            fx.ShakeScreen(0.2f, 0.4f);
                            bossAttackType = 0; attackTimer = 2.0f;
                        }
                    }
                    // 3. 毒沼・胞子エリア展開
                    else if (bossAttackType == 3) {
                        if (bossActionTimer <= 0.0f) {
                            animFrameCounter = 0; AudioManager::PlaySE(SE_SKILL);
                            fx.ShakeScreen(0.35f, 0.6f);

                            fx.SpawnHazardArea(p.position, 2.8f, 8.0f, 16.0f + level * 2.0f, GREEN, ELEM_ROT);

                            for (int k = 0; k < 2; k++) {
                                float offAngle = (float)GetRandomValue(0, 360) * DEG2RAD;
                                float offDist = (float)GetRandomValue(3, 7);
                                Vector3 randPos = Vector3Add(position, { cosf(offAngle) * offDist, 0, sinf(offAngle) * offDist });
                                if (!d.IsWall(randPos.x, randPos.z)) {
                                    fx.SpawnHazardArea(randPos, 2.5f, 8.0f, 14.0f + level * 1.8f, DARKGREEN, ELEM_ROT);
                                }
                            }

                            UI::AddSystemLog("★ POISON HAZARD AREA CREATED! ★", GREEN);
                            bossAttackType = 0; attackTimer = 2.2f;
                        }
                    }
                    // 4. 超広域ポイズンブラスト
                    else if (bossAttackType == 4) {
                        if (bossActionTimer <= 0.0f) {
                            animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                            for (int i = 0; i < 12; i++) {
                                float angle = i * 30.0f * DEG2RAD; Vector3 dir = { cosf(angle), 0.0f, sinf(angle) };
                                fx.SpawnEffect(Vector3Add(position, { 0, 0.5f, 0 }), dir, FX_SMASH, GREEN);
                            }
                            fx.ShakeScreen(0.4f, 0.7f);

                            float aoeRadius = 7.5f;
                            if (Vector3Distance(position, p.position) < aoeRadius) {
                                float rawDmg = 26.0f + level * 2.8f;
                                float defDmg = fmaxf(2.0f, rawDmg - p.defense);
                                float resist = p.GetPlayerElementResistance(ELEM_ROT);
                                float finalDmg = fmaxf(2.0f, defDmg * (1.0f - resist));
                                p.hp -= finalDmg;

                                fx.SpawnDamageText(p.position, (int)finalDmg);
                                fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                                fx.TriggerDamageFlash(0.35f);

                                std::string aoeFmt = DataManager::uiStrings.count("LOG_BOSS_AOE") ? DataManager::uiStrings["LOG_BOSS_AOE"] : "BOSS EXPLOSION HIT: %d DMG!";
                                UI::AddSystemLog(TextFormat(aoeFmt.c_str(), (int)finalDmg), RED);

                                // プレイヤーへの溶解デバフ付与
                                if (GetRandomValue(1, 100) <= 40) {
                                    if (p.HasStatusImmunity(ELEM_ROT)) {
                                        std::string immFmt = DataManager::uiStrings.count("LOG_STATUS_IMMUNE") ? DataManager::uiStrings["LOG_STATUS_IMMUNE"] : "★ STATUS IMMUNED! ★";
                                        UI::AddSystemLog(immFmt, GREEN);
                                    }
                                    else {
                                        p.status.ApplyStatus(ELEM_ROT, 3.5f, finalDmg);
                                        std::string rotKey = "LOG_PLAYER_ROT";
                                        if (DataManager::uiStrings.count(rotKey)) {
                                            UI::AddSystemLog(DataManager::uiStrings[rotKey], Player::GetElementColor(ELEM_ROT));
                                        }
                                    }
                                }
                            }
                            bossAttackType = 0; attackTimer = 2.5f;
                        }
                    }
                }
                // 通常ボスの攻撃処理
                else {
                    if (bossAttackType == 1) {
                        bossActionTimer -= dt;
                        if (bossActionTimer <= 0.0f) {
                            animFrameCounter = 0; bossComboStep++; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                            Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });
                            Color atkCol = (data.element != ELEM_NONE) ? Player::GetElementColor(data.element) : GOLD;
                            fx.SpawnEffect(spawnPos, bossTargetDir, FX_SLASH, atkCol);

                            Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                            float distToPlayer = Vector2Distance({ hitCenter.x, hitCenter.z }, { p.position.x, p.position.z });

                            if (distToPlayer < 2.6f) {
                                float rawDmg = 14.0f + level * 2.4f; if (bossComboStep == 3) rawDmg *= 1.5f;
                                float defDmg = fmaxf(2.0f, rawDmg - p.defense);
                                float resist = p.GetPlayerElementResistance(data.element);
                                float finalDmg = fmaxf(2.0f, defDmg * (1.0f - resist));
                                p.hp -= finalDmg;

                                fx.SpawnDamageText(p.position, (int)finalDmg);
                                fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                                if (resist > 0.05f) {
                                    std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                    UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                                }
                                else {
                                    std::string comboFmt = DataManager::uiStrings.count("LOG_BOSS_COMBO") ? DataManager::uiStrings["LOG_BOSS_COMBO"] : "BOSS COMBO HIT: %d DMG!";
                                    UI::AddSystemLog(TextFormat(comboFmt.c_str(), (int)finalDmg), RED);
                                }

                                // 属性ボスの近接攻撃による状態異常付与
                                if (data.element != ELEM_NONE && GetRandomValue(1, 100) <= 30) {
                                    if (p.HasStatusImmunity(data.element)) {
                                        std::string immFmt = DataManager::uiStrings.count("LOG_STATUS_IMMUNE") ? DataManager::uiStrings["LOG_STATUS_IMMUNE"] : "★ STATUS IMMUNED! ★";
                                        UI::AddSystemLog(immFmt, GREEN);
                                    }
                                    else {
                                        p.status.ApplyStatus(data.element, 3.5f, finalDmg);
                                        std::string logKey = "";
                                        if (data.element == ELEM_HELLFIRE) logKey = "LOG_PLAYER_BURN";
                                        else if (data.element == ELEM_ROT) logKey = "LOG_PLAYER_ROT";
                                        else if (data.element == ELEM_SOUL) logKey = "LOG_PLAYER_CHILL";
                                        else if (data.element == ELEM_ABYSS) logKey = "LOG_PLAYER_CURSE";
                                        if (!logKey.empty() && DataManager::uiStrings.count(logKey)) {
                                            UI::AddSystemLog(DataManager::uiStrings[logKey], Player::GetElementColor(data.element));
                                        }
                                    }
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
                    // 通常ボス 3方向弾幕（威力スケール化: 22.0f + level * 2.5f）
                    else if (bossAttackType == 2) {
                        bossActionTimer -= dt;
                        if (bossActionTimer <= 0.0f) {
                            animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                            Vector3 spawnPos = Vector3Add(position, { 0, 0.75f, 0 });
                            float bDmg = 22.0f + level * 2.5f;
                            int elem = (data.element != ELEM_NONE) ? data.element : ELEM_ABYSS;

                            for (int i = -1; i <= 1; i++) {
                                float angle = i * 20.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                                Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                                fx.SpawnProjectile(spawnPos, dir, 12.0f, 1, false, bDmg, elem);
                            }
                            bossAttackType = 0; attackTimer = 1.5f;
                        }
                    }
                    else if (bossAttackType == 3) {
                        bossActionTimer -= dt;
                        if (bossComboStep == 0) {
                            if (bossActionTimer <= 0.0f) { bossComboStep = 1; bossActionTimer = 0.6f; animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK); }
                        }
                        else if (bossComboStep == 1) {
                            float oldSpeed = speed; speed = speed * 3.5f;
                            bool hitWall = MoveSmart(Vector3Add(position, Vector3Scale(bossTargetDir, 10.0f)), d);
                            speed = oldSpeed;

                            float distToPlayer = Vector2Distance({ position.x, position.z }, { p.position.x, p.position.z });
                            if (distToPlayer < radius + p.radius + 0.8f) {
                                float rawDmg = 20.0f + level * 2.8f;
                                float defDmg = fmaxf(2.0f, rawDmg - p.defense);
                                float resist = p.GetPlayerElementResistance(data.element);
                                float finalDmg = fmaxf(2.0f, defDmg * (1.0f - resist));
                                p.hp -= finalDmg;

                                fx.SpawnDamageText(p.position, (int)finalDmg);
                                fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                                if (resist > 0.05f) {
                                    std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                    UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                                }
                                else {
                                    std::string dashFmt = DataManager::uiStrings.count("LOG_BOSS_DASH") ? DataManager::uiStrings["LOG_BOSS_DASH"] : "BOSS TACKLE HIT: %d DMG!";
                                    UI::AddSystemLog(TextFormat(dashFmt.c_str(), (int)finalDmg), RED);
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
                                float rawDmg = 25.0f + level * 2.8f;
                                float defDmg = fmaxf(2.0f, rawDmg - p.defense);
                                float resist = p.GetPlayerElementResistance(data.element);
                                float finalDmg = fmaxf(2.0f, defDmg * (1.0f - resist));
                                p.hp -= finalDmg;

                                fx.SpawnDamageText(p.position, (int)finalDmg);
                                fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                                if (resist > 0.05f) {
                                    std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                    UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                                }
                                else {
                                    std::string aoeFmt = DataManager::uiStrings.count("LOG_BOSS_AOE") ? DataManager::uiStrings["LOG_BOSS_AOE"] : "BOSS EXPLOSION HIT: %d DMG!";
                                    UI::AddSystemLog(TextFormat(aoeFmt.c_str(), (int)finalDmg), RED);
                                }

                                fx.TriggerDamageFlash(0.35f);
                            }
                            bossAttackType = 0; attackTimer = 2.5f;
                        }
                    }
                }
            }
        }
        else {
            if (!IsPlant()) {
                if (Vector3Distance(position, patrolTarget) < 1.2f) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
                bool hitWall = MoveSmart(patrolTarget, d);
                if (hitWall) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
                if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++; else stuckCount = 0;
                if (stuckCount > 60) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            }
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
            if (!isChargingAttack && eType != E_TRAP && !IsPlant()) {
                if (dist > attackRange * 0.7f) MoveSmart(p.position, d);
            }

            if (dist < attackRange && attackTimer <= 0.0f && !isChargingAttack) {
                isChargingAttack = true;
                lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));

                if (IsPlant()) {
                    attackChargeMax = 0.75f;
                    normalAttackPattern = GetRandomValue(1, 3);
                }
                else {
                    if (eType == E_AXE) attackChargeMax = 0.85f;
                    else if (eType == E_SPEAR) attackChargeMax = 0.65f;
                    else attackChargeMax = 0.5f;
                }

                attackChargeTimer = attackChargeMax;
            }

            if (isChargingAttack) {
                attackChargeTimer -= dt;
                lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));

                if (attackChargeTimer <= 0.0f) {
                    isChargingAttack = false;
                    attackAnimTimer = 0.5f;
                    animFrameCounter = 0;

                    Vector3 spawnPos = Vector3Add(position, { 0, 0.75f, 0 });
                    AudioManager::PlaySE(SE_ENEMY_ATTACK);

                    // ★ 通常植物敵の弾丸威力スケール化
                    if (IsPlant()) {
                        if (normalAttackPattern == 1) {
                            float pDmg = 18.0f + level * 2.2f;
                            fx.SpawnProjectile(spawnPos, lastAttackDir, 16.0f, 2, false, pDmg, ELEM_ROT);
                            attackTimer = 1.8f;
                        }
                        else if (normalAttackPattern == 2) {
                            float pDmg = 14.0f + level * 1.8f;
                            for (int i = -1; i <= 1; i++) {
                                float angle = i * 18.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                                Vector3 dir = { lastAttackDir.x * c - lastAttackDir.z * s, 0.0f, lastAttackDir.x * s + lastAttackDir.z * c };
                                fx.SpawnProjectile(spawnPos, dir, 10.0f, 2, false, pDmg, ELEM_ROT);
                            }
                            attackTimer = 2.0f;
                        }
                        else {
                            fx.SpawnHazardArea(p.position, 1.8f, 6.0f, 10.0f + level * 1.5f, GREEN, ELEM_ROT);
                            fx.SpawnEffect(p.position, { 0, 1, 0 }, FX_SMASH, DARKGREEN, 1.8f);
                            attackTimer = 2.4f;
                        }
                    }
                    // ★ 通常アーチャーの弾丸威力スケール化
                    else if (eType == E_ARCHER) {
                        float pDmg = 16.0f + level * 2.2f;
                        fx.SpawnProjectile(spawnPos, lastAttackDir, 18.0f, 0, false, pDmg, data.element);
                        attackTimer = 1.8f;
                    }
                    // ★ 通常メイジの弾丸威力スケール化
                    else if (eType == E_MAGE) {
                        float pDmg = 19.0f + level * 2.5f;
                        int elem = (data.element != ELEM_NONE) ? data.element : ELEM_ABYSS;
                        fx.SpawnProjectile(spawnPos, lastAttackDir, 8.5f, 1, false, pDmg, elem);
                        attackTimer = 2.2f;
                    }
                    else if (eType == E_TRAP) {
                        float pDmg = 20.0f + level * 2.0f;
                        fx.SpawnProjectile(spawnPos, lastAttackDir, 12.0f, 0, false, pDmg, data.element);
                        attackTimer = 2.5f;
                    }
                    else {
                        EffectType effectType = FX_SLASH;
                        if (eType == E_SPEAR) effectType = FX_THRUST;
                        else if (eType == E_AXE) effectType = FX_SMASH;

                        Color atkCol = (data.element != ELEM_NONE) ? Player::GetElementColor(data.element) : GOLD;
                        fx.SpawnEffect(spawnPos, lastAttackDir, effectType, atkCol);

                        float aoeRadius = attackRange * 0.9f;
                        Vector3 hitCenter = Vector3Add(position, Vector3Scale(lastAttackDir, attackRange * 0.5f));

                        if (Vector3Distance(hitCenter, p.position) < aoeRadius) {
                            float rawDmg = 12.0f + level * 2.2f;
                            rawDmg *= (1.0f - status.GetAttackReductionRate());
                            float defDmg = fmaxf(2.0f, rawDmg - p.defense);

                            float resist = p.GetPlayerElementResistance(data.element);
                            float finalDmg = fmaxf(2.0f, defDmg * (1.0f - resist));
                            p.hp -= finalDmg;

                            fx.SpawnDamageText(p.position, (int)finalDmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);

                            if (resist > 0.05f) {
                                std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_HIT") ? DataManager::uiStrings["LOG_RESISTED_HIT"] : "★ RESISTED! (-%d%%) ★";
                                UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                            }
                            else {
                                std::string takenFmt = DataManager::uiStrings.count("LOG_DMG_TAKEN") ? DataManager::uiStrings["LOG_DMG_TAKEN"] : "%s HIT: %d DMG!";
                                UI::AddSystemLog(TextFormat(takenFmt.c_str(), data.name.c_str(), (int)finalDmg), RED);
                            }

                            // 通常近接敵の属性攻撃による状態異常付与
                            if (data.element != ELEM_NONE && GetRandomValue(1, 100) <= 30) {
                                if (p.HasStatusImmunity(data.element)) {
                                    std::string immFmt = DataManager::uiStrings.count("LOG_STATUS_IMMUNE") ? DataManager::uiStrings["LOG_STATUS_IMMUNE"] : "★ STATUS IMMUNED! ★";
                                    UI::AddSystemLog(immFmt, GREEN);
                                }
                                else {
                                    p.status.ApplyStatus(data.element, 3.5f, finalDmg);
                                    std::string logKey = "";
                                    if (data.element == ELEM_HELLFIRE) logKey = "LOG_PLAYER_BURN";
                                    else if (data.element == ELEM_ROT) logKey = "LOG_PLAYER_ROT";
                                    else if (data.element == ELEM_SOUL) logKey = "LOG_PLAYER_CHILL";
                                    else if (data.element == ELEM_ABYSS) logKey = "LOG_PLAYER_CURSE";
                                    if (!logKey.empty() && DataManager::uiStrings.count(logKey)) {
                                        UI::AddSystemLog(DataManager::uiStrings[logKey], Player::GetElementColor(data.element));
                                    }
                                }
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
            if (!IsPlant()) {
                if (Vector3Distance(position, patrolTarget) < 1.2f) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
                bool hitWall = MoveSmart(patrolTarget, d);
                if (hitWall) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
                if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++; else stuckCount = 0;
                if (stuckCount > 60) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            }
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

        int animIndex = 2;
        float comboProgress = 1.0f - (bossActionTimer / 0.55f);

        if (isDying) {
            animIndex = 1;
        }
        else if (isBoss) {
            if (IsPlant()) {
                animIndex = (isChargingAttack || bossAttackType != 0) ? 0 : 2;
            }
            else {
                if (bossAttackType == 1) {
                    animIndex = (comboProgress < 0.70f) ? 2 : 0;
                }
                else if (bossAttackType == 2 || bossAttackType == 4) animIndex = 2;
                else if (bossAttackType == 3) animIndex = (bossComboStep == 1) ? 3 : 2;
                else {
                    float distToPlayer = Vector3Distance(position, playerPos);
                    if (state == STATE_PATROL) animIndex = 3;
                    else if (state == STATE_ATTACK && distToPlayer > 3.0f) animIndex = 3;
                    else animIndex = 2;
                }
            }
        }
        else {
            if (attackAnimTimer > 0.0f) animIndex = 0;
            else if (isChargingAttack) animIndex = 2;
            else if (!IsPlant() && (state == STATE_CHASE || state == STATE_PATROL)) animIndex = 3;
            else animIndex = 2;
        }

        if (animIndex >= gm.animCount) animIndex = 0;
        int currentAnimIndex = animIndex;

        ModelAnimation anim = gm.anims[currentAnimIndex];
        int frame = 0;

        if (isDying) {
            frame = (animFrameCounter >= anim.frameCount - 1) ? anim.frameCount - 1 : animFrameCounter;
        }
        else if (!IsPlant() && isBoss && bossAttackType == 1 && comboProgress >= 0.70f) {
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

            targetDir.y = 0.0f;
            if (Vector3Length(targetDir) > 0.01f) {
                rotationAngle = atan2f(targetDir.x, targetDir.z) * RAD2DEG;
            }
        }

        float scale = isBoss ? 0.015f : 0.01f;
        Vector3 drawPos = { position.x, position.y - (isBoss ? 0.6f : 0.4f), position.z };

        Matrix matRotX = MatrixRotateX(-90.0f * DEG2RAD);
        Matrix matRotY = MatrixRotateY(rotationAngle * DEG2RAD);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotX);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotY);

        DrawModel(gm.model, drawPos, scale, data.tint);

        if (!isDying && status.HasAnyStatus()) {
            status.DrawAura(position, radius);
        }

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
        Color c = IsPlant() ? DARKGREEN : ((eType == E_SWORD) ? MAROON : (eType == E_SPEAR) ? ORANGE : (eType == E_AXE) ? PURPLE : (eType == E_ARCHER) ? DARKGREEN : (eType == E_MAGE) ? PINK : (eType == E_TRAP) ? DARKGRAY : GRAY);
        Vector3 drawPos = { position.x, position.y - 0.4f, position.z };
        DrawCube(drawPos, 1, 1.2f, 1, c); DrawCubeWires(drawPos, 1, 1.2f, 1, BLACK);
    }

    if (debug) {
        Vector3 headPos = Vector3Add(position, { 0, 2.5f, 0 });
        Color debugColor = (animFrameCounter % 10 < 5) ? GREEN : YELLOW;
        DrawSphere(headPos, 0.2f, debugColor);
        if (hasModel) DrawCubeWires(position, 2.0f, 2.0f, 2.0f, RED);
    }

    // 通常敵のAoE予兆描画
    if (!isDying && !isBoss && isChargingAttack) {
        float progress = 1.0f - (attackChargeTimer / attackChargeMax);
        progress = fmaxf(0.0f, fminf(1.0f, progress));

        if (IsPlant() && normalAttackPattern == 3) {
            Vector3 trapPos = playerPos; trapPos.y = 0.12f;
            DrawCylinderWires(trapPos, 1.8f, 1.8f, 0.04f, 24, Fade(GREEN, 0.9f));
            DrawCylinder(trapPos, 1.8f * progress, 1.8f * progress, 0.03f, 24, Fade(LIME, 0.4f));
        }
        else if (eType == E_ARCHER || eType == E_MAGE || eType == E_TRAP || (IsPlant() && normalAttackPattern == 1)) {
            float totalLen = 14.0f; float w = 0.8f;
            Vector3 dir = Vector3Normalize(lastAttackDir);
            Vector3 side = { -dir.z, 0.0f, dir.x };
            Color laneCol = IsPlant() ? GREEN : RED;

            Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
            Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
            Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
            Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
            p1.y = p2.y = p3.y = p4.y = 0.12f;

            DrawLine3D(p1, p2, Fade(laneCol, 0.8f)); DrawLine3D(p2, p3, Fade(laneCol, 0.8f));
            DrawLine3D(p3, p4, Fade(laneCol, 0.8f)); DrawLine3D(p4, p1, Fade(laneCol, 0.8f));

            float chargeLen = totalLen * progress;
            Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
            Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
            cp2.y = cp3.y = 0.13f;
            DrawTriangle3D(p1, p4, cp3, Fade(laneCol, 0.35f)); DrawTriangle3D(p1, cp3, cp2, Fade(laneCol, 0.35f));
            DrawTriangle3D(cp3, p4, p1, Fade(laneCol, 0.35f)); DrawTriangle3D(cp2, cp3, p1, Fade(laneCol, 0.35f));
        }
        else if (IsPlant() && normalAttackPattern == 2) {
            for (int i = -1; i <= 1; i++) {
                float angle = i * 18.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                Vector3 dir = { lastAttackDir.x * c - lastAttackDir.z * s, 0.0f, lastAttackDir.x * s + lastAttackDir.z * c };
                float totalLen = 10.0f; float w = 0.6f;
                Vector3 side = { -dir.z, 0.0f, dir.x };

                Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
                Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
                Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                p1.y = p2.y = p3.y = p4.y = 0.12f;

                DrawLine3D(p1, p2, Fade(LIME, 0.7f)); DrawLine3D(p2, p3, Fade(LIME, 0.7f));
                DrawLine3D(p3, p4, Fade(LIME, 0.7f)); DrawLine3D(p4, p1, Fade(LIME, 0.7f));
            }
        }
        else if (eType == E_SWORD || eType == E_AXE || eType == E_SPEAR) {
            float aoeRadius = attackRange * 0.9f;
            Vector3 hitCenter = Vector3Add(position, Vector3Scale(lastAttackDir, attackRange * 0.5f));
            hitCenter.y = 0.12f;

            DrawCylinderWires(hitCenter, aoeRadius, aoeRadius, 0.04f, 32, Fade(RED, 0.95f));
            float fillAlpha = 0.10f + (progress * 0.55f);
            DrawCylinder(hitCenter, aoeRadius, aoeRadius, 0.03f, 32, Fade(RED, fillAlpha));
            float gaugeRadius = aoeRadius * progress;
            DrawCylinder(hitCenter, gaugeRadius, gaugeRadius, 0.05f, 32, Fade(ORANGE, 0.5f + progress * 0.4f));
        }
    }

    // ボスのAoE予兆描画
    if (!isDying && isBoss && bossAttackType != 0) {
        float maxTime = 1.0f;
        if (IsPlant()) {
            if (bossAttackType == 1) maxTime = 0.75f;
            else if (bossAttackType == 2) maxTime = 1.1f;
            else if (bossAttackType == 3) maxTime = 0.9f;
            else if (bossAttackType == 4) maxTime = 1.8f;
        }
        else {
            if (bossAttackType == 1) maxTime = 0.55f;
            else if (bossAttackType == 2) maxTime = 0.9f;
            else if (bossAttackType == 3) maxTime = 1.0f;
            else if (bossAttackType == 4) maxTime = 2.0f;
        }

        bool showTelegraph = true;
        if (!IsPlant() && bossAttackType == 3 && bossComboStep == 1) showTelegraph = false;

        if (showTelegraph) {
            float progress = 1.0f - (bossActionTimer / maxTime);
            progress = fmaxf(0.0f, fminf(1.0f, progress));

            Color outlineColor = IsPlant() ? Fade(LIME, 0.95f) : Fade(RED, 0.95f);
            Color fillColor = IsPlant() ? Fade(DARKGREEN, 0.3f + progress * 0.4f) : Fade(PURPLE, 0.12f + progress * 0.45f);
            Color chargeColor = IsPlant() ? Fade(GREEN, 0.5f + progress * 0.4f) : Fade(RED, 0.45f + progress * 0.45f);

            if (IsPlant() && bossAttackType == 1) {
                for (int i = -2; i <= 2; i++) {
                    float angle = i * 16.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                    Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                    float totalLen = 16.0f; float w = 1.2f;
                    Vector3 side = { -dir.z, 0.0f, dir.x };

                    Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                    Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
                    Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
                    Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                    p1.y = p2.y = p3.y = p4.y = 0.12f;

                    DrawLine3D(p1, p2, outlineColor); DrawLine3D(p2, p3, outlineColor);
                    DrawLine3D(p3, p4, outlineColor); DrawLine3D(p4, p1, outlineColor);

                    float chargeLen = totalLen * progress;
                    Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
                    Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
                    cp2.y = cp3.y = 0.13f;
                    DrawTriangle3D(p1, p4, cp3, chargeColor); DrawTriangle3D(p1, cp3, cp2, chargeColor);
                    DrawTriangle3D(cp3, p4, p1, chargeColor); DrawTriangle3D(cp2, cp3, p1, chargeColor);
                }
            }
            else if (IsPlant() && bossAttackType == 2) {
                float totalRadius = 10.0f;
                Vector3 center = { position.x, 0.12f, position.z };
                DrawCylinderWires(center, totalRadius, totalRadius, 0.04f, 36, outlineColor);
                DrawCylinderWires(center, totalRadius * progress, totalRadius * progress, 0.05f, 36, chargeColor);
                DrawCylinder(center, totalRadius * progress, totalRadius * progress, 0.02f, 36, Fade(LIME, 0.25f));
            }
            else if (IsPlant() && bossAttackType == 3) {
                Vector3 center = { playerPos.x, 0.12f, playerPos.z };
                float r = 2.8f;
                DrawCylinderWires(center, r, r, 0.04f, 32, outlineColor);
                DrawCylinder(center, r * progress, r * progress, 0.04f, 32, chargeColor);
            }
            else if (bossAttackType == 4) {
                float totalRadius = 7.5f;
                Vector3 center = { position.x, 0.12f, position.z };
                DrawCylinder(center, totalRadius, totalRadius, 0.04f, 36, fillColor);
                DrawCylinder(center, totalRadius * progress, totalRadius * progress, 0.05f, 36, chargeColor);
                DrawCylinderWires(center, totalRadius, totalRadius, 0.04f, 36, outlineColor);
            }
            else if (!IsPlant()) {
                if (bossAttackType == 1) {
                    float comboRadius = 2.5f;
                    Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                    hitCenter.y = 0.12f;
                    DrawCylinder(hitCenter, comboRadius, comboRadius, 0.04f, 32, Fade(RED, 0.3f));
                    DrawCylinder(hitCenter, comboRadius * progress, comboRadius * progress, 0.05f, 32, Fade(ORANGE, 0.7f));
                    DrawCylinderWires(hitCenter, comboRadius, comboRadius, 0.04f, 32, outlineColor);
                }
                else if (bossAttackType == 2) {
                    for (int i = -1; i <= 1; i++) {
                        float angle = i * 20.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                        Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                        float totalLen = 18.0f; float w = 1.4f;
                        Vector3 side = { -dir.z, 0.0f, dir.x };

                        Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                        Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
                        Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
                        Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                        p1.y = p2.y = p3.y = p4.y = 0.12f;

                        DrawLine3D(p1, p2, outlineColor); DrawLine3D(p2, p3, outlineColor);
                        DrawLine3D(p3, p4, outlineColor); DrawLine3D(p4, p1, outlineColor);

                        float chargeLen = totalLen * progress;
                        Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
                        Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
                        cp2.y = cp3.y = 0.13f;
                        DrawTriangle3D(p1, p4, cp3, chargeColor); DrawTriangle3D(p1, cp3, cp2, chargeColor);
                        DrawTriangle3D(cp3, p4, p1, chargeColor); DrawTriangle3D(cp2, cp3, p1, chargeColor);
                    }
                }
                else if (bossAttackType == 3) {
                    float totalLen = 12.75f; float w = 5.5f;
                    Vector3 dir = Vector3Normalize(bossTargetDir);
                    Vector3 side = { -dir.z, 0.0f, dir.x };

                    Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                    Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
                    Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
                    Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                    p1.y = p2.y = p3.y = p4.y = 0.12f;

                    DrawLine3D(p1, p2, outlineColor); DrawLine3D(p2, p3, outlineColor);
                    DrawLine3D(p3, p4, outlineColor); DrawLine3D(p4, p1, outlineColor);

                    float chargeLen = totalLen * progress;
                    Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
                    Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
                    cp2.y = cp3.y = 0.13f;
                    DrawTriangle3D(p1, p4, cp3, chargeColor); DrawTriangle3D(p1, cp3, cp2, chargeColor);
                    DrawTriangle3D(cp3, p4, p1, chargeColor); DrawTriangle3D(cp2, cp3, p1, chargeColor);
                }
            }
        }
    }
}