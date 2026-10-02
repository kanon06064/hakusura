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
                        fx.SpawnEffect(spawnPos, bossTargetDir, FX_SLASH, GOLD);

                        Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                        if (Vector3Distance(hitCenter, p.position) < 2.5f) {
                            float rawDmg = 10.0f + level * 2; if (bossComboStep == 3) rawDmg *= 1.5f;
                            float dmg = fmaxf(1.0f, rawDmg - p.defense); p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg); fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_COMBO"].c_str(), (int)dmg), RED);
                            fx.TriggerDamageFlash(0.25f);

                            if (bossComboStep >= 3) fx.ShakeScreen(0.3f, 0.6f);
                            else fx.ShakeScreen(0.15f, 0.35f);
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
                            float rawDmg = 15.0f + level * 2; float dmg = fmaxf(1.0f, rawDmg - p.defense); p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg); fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_DASH"].c_str(), (int)dmg), RED);
                            fx.TriggerDamageFlash(0.35f);
                            fx.ShakeScreen(0.35f, 0.7f);
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
                            float rawDmg = 20.0f + level * 2; float dmg = fmaxf(1.0f, rawDmg - p.defense); p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg); fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_AOE"].c_str(), (int)dmg), RED);
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
    // 通常敵AI（攻撃予兆＋チャージシステム搭載）
    // =========================================================================
    else {
        if (dist < attackRange && canSee && dist < effectiveDetect) state = STATE_ATTACK;
        else if (dist < effectiveDetect && canSee) state = STATE_CHASE;
        else state = STATE_PATROL;

        if (state == STATE_CHASE || state == STATE_ATTACK) {
            stuckCount = 0;
            // チャージ中でない時はプレイヤーを追う
            if (!isChargingAttack && eType != E_TRAP) {
                if (dist > attackRange * 0.7f) MoveSmart(p.position, d);
            }

            // 攻撃範囲に入ったら、即座に殴るのではなく「予兆（チャージ）を開始」する
            if (dist < attackRange && attackTimer <= 0.0f && !isChargingAttack) {
                isChargingAttack = true;
                lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));

                // 武器種によってタメ時間を設定（重い武器ほど予兆が長い）
                if (eType == E_AXE) attackChargeMax = 0.85f;
                else if (eType == E_SPEAR) attackChargeMax = 0.65f;
                else attackChargeMax = 0.5f;

                attackChargeTimer = attackChargeMax;
            }

            // チャージ中の処理（タイマー進行 → 0になったら攻撃発動！）
            if (isChargingAttack) {
                attackChargeTimer -= dt;
                // チャージ中はプレイヤーの方向を見据える
                lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));

                if (attackChargeTimer <= 0.0f) {
                    isChargingAttack = false;
                    attackAnimTimer = 0.5f; // 攻撃モーション再生開始
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

                        fx.SpawnEffect(spawnPos, lastAttackDir, effectType, GOLD);

                        // 予兆（AoE）の範囲内にプレイヤーがまだ残っていればダメージ
                        float aoeRadius = attackRange * 0.9f;
                        Vector3 hitCenter = Vector3Add(position, Vector3Scale(lastAttackDir, attackRange * 0.5f));

                        if (Vector3Distance(hitCenter, p.position) < aoeRadius) {
                            float rawDmg = 10.0f + level * 2;
                            float dmg = fmaxf(1.0f, rawDmg - p.defense);
                            p.hp -= dmg;

                            fx.SpawnDamageText(p.position, (int)dmg);
                            fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_DMG_TAKEN"].c_str(), data.name.c_str(), (int)dmg), RED);
                            fx.TriggerDamageFlash(0.22f);

                            if (eType == E_AXE) fx.ShakeScreen(0.2f, 0.45f);
                            else fx.ShakeScreen(0.15f, 0.3f);
                        }

                        attackTimer = 1.5f; // 攻撃後のクールタイム
                    }
                }
            }
        }
        else {
            isChargingAttack = false; // プレイヤーを見失ったらチャージ解除
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

        // =====================================================================
        // 1. アニメーション番号の決定
        // =====================================================================
        int animIndex = 2; // デフォルトは待機(Idle)
        float comboProgress = 1.0f - (bossActionTimer / 0.55f); // ボスコンボ進行度 (0.0 〜 1.0)

        if (isDying) {
            animIndex = 1; // 死亡モーション
        }
        else if (isBoss) {
            if (bossAttackType == 1) {
             
                animIndex = (comboProgress < 0.70f) ? 2 : 0;
            }
            else if (bossAttackType == 2 || bossAttackType == 4) {
                animIndex = 2; // 魔法弾幕・AoEタメ中は待機ポーズ
            }
            else if (bossAttackType == 3) {
                animIndex = (bossComboStep == 1) ? 3 : 2; // 突進突撃中は走り(3)
            }
            else {
                // bossAttackType == 0（攻撃間の移動・アプローチフェーズ）
                float distToPlayer = Vector3Distance(position, playerPos);
                if (state == STATE_PATROL) {
                    animIndex = 3; // パトロール徘徊中は歩き/走り
                }
                else if (state == STATE_ATTACK && distToPlayer > 3.0f) {
                    animIndex = 3; // プレイヤーに歩み寄ってくる時は走り！
                }
                else {
                    animIndex = 2; // 3m以内の近距離では待機(Idle)
                }
            }
        }
        else {
            if (attackAnimTimer > 0.0f) {
                animIndex = 0; // 攻撃振り下ろしモーション
            }
            else if (isChargingAttack) {
                animIndex = 2; // チャージ中は武器を構えて待機
            }
            else if (state == STATE_CHASE || state == STATE_PATROL) {
                animIndex = 3; // 走り
            }
            else {
                animIndex = 2; // クールタイム中の待機
            }
        }

        if (animIndex >= gm.animCount) animIndex = 0;
        int currentAnimIndex = animIndex;

        // =====================================================================
        // 2. フレーム番号の決定（時間と完全同期）
        // =====================================================================
        ModelAnimation anim = gm.anims[currentAnimIndex];
        int frame = 0;

        if (isDying) {
            frame = (animFrameCounter >= anim.frameCount - 1) ? anim.frameCount - 1 : animFrameCounter;
        }
        //  ボスのコンボ斬撃
        else if (isBoss && bossAttackType == 1 && comboProgress >= 0.70f) {
            float swingProgress = (comboProgress - 0.45f) / 0.55f;
            if (swingProgress > 1.0f) swingProgress = 1.0f;
            frame = (int)(swingProgress * (float)(anim.frameCount - 1));
        }
        // 通常敵の攻撃：attackAnimTimer（0.5秒）に合わせて1回きっちり振り下ろす
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

        // =====================================================================
        // 3. 向き（回転角度）の計算
        // =====================================================================
        float rotationAngle = 0.0f;
        Vector3 targetDir = { 0, 0, 1 };

        if (!isDying) {
            if (isBoss) {
                if (bossAttackType != 0) {
                    targetDir = lastAttackDir;
                }
                else if (state == STATE_ATTACK || state == STATE_CHASE) {
                    targetDir = Vector3Subtract(playerPos, position);
                }
                else {
                    targetDir = Vector3Subtract(patrolTarget, position);
                }
            }
            else {
                if (isChargingAttack) {
                    targetDir = lastAttackDir;
                }
                else if (state == STATE_CHASE || state == STATE_ATTACK) {
                    targetDir = Vector3Subtract(playerPos, position); // プレイヤーを正面に見据える
                }
                else {
                    targetDir = Vector3Subtract(patrolTarget, position);
                }
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

        // =====================================================================
        // 4. 武器ボーン追従アタッチメント
        // =====================================================================
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
    // 5. 通常敵のAoE予兆表示（間合いに入ったら枠線出現＋中身が満ちる）
    // =========================================================================
    if (!isDying && !isBoss && isChargingAttack && (eType == E_SWORD || eType == E_AXE || eType == E_SPEAR)) {
        float progress = 1.0f - (attackChargeTimer / attackChargeMax);
        if (progress < 0.0f) progress = 0.0f;
        if (progress > 1.0f) progress = 1.0f;

        float aoeRadius = attackRange * 0.9f;
        Vector3 hitCenter = Vector3Add(position, Vector3Scale(lastAttackDir, attackRange * 0.5f));
        hitCenter.y = 0.12f;

        // 1. はっきり見える外枠の線
        DrawCylinderWires(hitCenter, aoeRadius, aoeRadius, 0.04f, 32, Fade(RED, 0.95f));

        // 2. 中身の色：時間経過で薄い赤から濃い赤へ満ちていく
        float fillAlpha = 0.10f + (progress * 0.55f);
        DrawCylinder(hitCenter, aoeRadius, aoeRadius, 0.03f, 32, Fade(RED, fillAlpha));

        // 3. 攻撃までの時間を直感的に伝えるチャージゲージ（中心から外枠へ広がる円）
        float gaugeRadius = aoeRadius * progress;
        DrawCylinder(hitCenter, gaugeRadius, gaugeRadius, 0.05f, 32, Fade(ORANGE, 0.5f + progress * 0.4f));
    }

    // =========================================================================
    // 6. ボスのAoE予兆表示（全パターンで枠線＋進行度ゲージが満ちる）
    // =========================================================================
    if (!isDying && isBoss && bossAttackType != 0) {
        float maxTime = 1.0f;
        if (bossAttackType == 1) maxTime = 0.55f; // ★ コンボ時間に統一
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

            // ① ボスの巨大全方位AoE（bossAttackType == 4）
            if (bossAttackType == 4) {
                float totalRadius = 7.0f;
                Vector3 center = { position.x, 0.12f, position.z };

                DrawCylinder(center, totalRadius, totalRadius, 0.04f, 36, fillColor);
                DrawCylinder(center, totalRadius * progress, totalRadius * progress, 0.05f, 36, chargeColor);
                DrawCylinderWires(center, totalRadius, totalRadius, 0.04f, 36, outlineColor);
                DrawCylinderWires(center, totalRadius + 0.05f, totalRadius + 0.05f, 0.04f, 36, Fade(RED, 0.6f));
            }
            // ② ボスのコンボ斬撃（bossAttackType == 1）
            else if (bossAttackType == 1) {
                float comboRadius = 2.5f;
                Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                hitCenter.y = 0.12f;

                DrawCylinder(hitCenter, comboRadius, comboRadius, 0.04f, 32, Fade(RED, baseAlpha));
                DrawCylinder(hitCenter, comboRadius * progress, comboRadius * progress, 0.05f, 32, Fade(ORANGE, 0.7f));
                DrawCylinderWires(hitCenter, comboRadius, comboRadius, 0.04f, 32, outlineColor);
            }
            // ③ ボスの3方向弾幕レーン（bossAttackType == 2）
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

                    // 背景の薄い塗り
                    DrawTriangle3D(p1, p4, p3, fillColor); DrawTriangle3D(p1, p3, p2, fillColor);
                    DrawTriangle3D(p3, p4, p1, fillColor); DrawTriangle3D(p2, p3, p1, fillColor);

                    // 手前から奥へ伸びていくチャージゲージ
                    float chargeLen = totalLen * progress;
                    Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
                    Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
                    cp2.y = cp3.y = 0.13f;
                    DrawTriangle3D(p1, p4, cp3, chargeColor); DrawTriangle3D(p1, cp3, cp2, chargeColor);
                    DrawTriangle3D(cp3, p4, p1, chargeColor); DrawTriangle3D(cp2, cp3, p1, chargeColor);

                    // クッキリした外枠線
                    DrawLine3D(p1, p2, outlineColor); DrawLine3D(p2, p3, outlineColor);
                    DrawLine3D(p3, p4, outlineColor); DrawLine3D(p4, p1, outlineColor);
                }
            }
            // ④ ボスの突進矩形レーン（bossAttackType == 3）
            else if (bossAttackType == 3) {
                float totalLen = 12.75f; float w = 5.5f;
                Vector3 dir = Vector3Normalize(bossTargetDir);
                Vector3 side = { -dir.z, 0.0f, dir.x };

                Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, w / 2.0f)));
                Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, totalLen), Vector3Scale(side, -w / 2.0f)));
                Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                p1.y = p2.y = p3.y = p4.y = 0.12f;

                // 背景の薄い塗り
                DrawTriangle3D(p1, p4, p3, fillColor); DrawTriangle3D(p1, p3, p2, fillColor);
                DrawTriangle3D(p3, p4, p1, fillColor); DrawTriangle3D(p2, p3, p1, fillColor);

                // 手前から奥へ伸びていくチャージ長方形
                float chargeLen = totalLen * progress;
                Vector3 cp2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, w / 2.0f)));
                Vector3 cp3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, chargeLen), Vector3Scale(side, -w / 2.0f)));
                cp2.y = cp3.y = 0.13f;
                DrawTriangle3D(p1, p4, cp3, chargeColor); DrawTriangle3D(p1, cp3, cp2, chargeColor);
                DrawTriangle3D(cp3, p4, p1, chargeColor); DrawTriangle3D(cp2, cp3, p1, chargeColor);

                // クッキリした外枠線
                DrawLine3D(p1, p2, outlineColor); DrawLine3D(p2, p3, outlineColor);
                DrawLine3D(p3, p4, outlineColor); DrawLine3D(p4, p1, outlineColor);
            }
        }
    }
}