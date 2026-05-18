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
    position = sp; data = d; eType = (EnemyType)d.type; state = STATE_PATROL; level = fl;

    maxHp = d.hp + (level * 10.0f); hp = maxHp;
    speed = d.speed; detectRange = d.detect; attackRange = d.atkRange;

    int levelBonus = (int)((float)d.exp * 0.1f * (float)level) + level;
    expValue = d.exp + levelBonus;

    radius = 0.45f; attackTimer = 0; hudTimer = 0;
    lastAttackDir = { 1,0,0 }; patrolTarget = sp;
    lastPos = sp; stuckCount = 0;
    animFrameCounter = GetRandomValue(0, 30); // アニメーションが全員揃わないよう開始をズラす

    isDying = false; isDead = false;
    isBoss = false; bossAttackType = 0; bossComboStep = 0; bossActionTimer = 0.0f; bossTargetDir = { 0,0,0 };
}

void Enemy::StartDeath() {
    if (isDying) return;
    isDying = true; animFrameCounter = 0;
    std::string key = data.modelName;
    if (key.empty() || DataManager::loadedModels.count(key) == 0) { isDead = true; } // モデルがなければ即死
}

void Enemy::ApplyKnockback(Vector3 dir, float f, Dungeon& d) {
    if (isDying || isBoss) return; // ボスはノックバック無効(スーパーアーマー)
    Vector3 kb = Vector3Scale(dir, f);
    if (!d.CheckCollisionRadius(Vector3Add(position, { kb.x, 0, 0 }), radius)) position.x += kb.x;
    if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, kb.z }), radius)) position.z += kb.z;
}

bool Enemy::MoveSmart(Vector3 target, Dungeon& d) {
    if (eType == E_TRAP) return false; // トラップ型は動かない

    Vector3 dir = Vector3Normalize(Vector3Subtract(target, position));
    Vector3 vel = Vector3Scale(dir, speed);
    bool hitWall = false;

    // XYZをそれぞれ個別に判定することで、壁にぶつかっても横にスライドして進むことができる
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
            if (gm.animCount > 1) { // 死亡アニメーション(Index=1)があれば最後まで再生する
                animFrameCounter++;
                if (animFrameCounter >= gm.anims[1].frameCount) { isDead = true; }
            }
            else { isDead = true; }
        }
        else { isDead = true; }
        return;
    }

    if (hudTimer > 0) hudTimer -= GetFrameTime();
    if (attackTimer > 0) attackTimer -= GetFrameTime();
    animFrameCounter++;

    float dist = Vector3Distance(position, p.position);
    bool canSee = d.HasLineOfSight(position, p.position);

    // プレイヤーが隠密(ステルス)状態の場合は、敵の視界が極端に狭くなる
    float effectiveDetect = p.isStealth ? 1.5f : detectRange;
    if (state == STATE_CHASE || state == STATE_ATTACK) { if (p.isStealth) effectiveDetect = 3.0f; }

    // --- ボス専用のAIループ ---
    if (isBoss) {
        if (dist < effectiveDetect && canSee) { state = STATE_ATTACK; }
        else { state = STATE_PATROL; }

        if (state == STATE_ATTACK) {
            stuckCount = 0; patrolTarget = p.position;

            if (bossAttackType == 0) { // まだ何も大技を出していない状態
                if (attackTimer <= 0.0f) {
                    // ランダムな大技(1~4)を選択し、予兆(タメ)を開始する
                    bossAttackType = GetRandomValue(1, 4); bossComboStep = 0; animFrameCounter = 0;
                    bossTargetDir = Vector3Normalize(Vector3Subtract(p.position, position));
                    lastAttackDir = bossTargetDir;

                    if (bossAttackType == 1) bossActionTimer = 0.5f; // 3連撃(タメ0.5秒)
                    else if (bossAttackType == 2) bossActionTimer = 0.8f; // 弾幕(タメ0.8秒)
                    else if (bossAttackType == 3) bossActionTimer = 1.0f; // 突進(タメ1.0秒)
                    else if (bossAttackType == 4) bossActionTimer = 1.8f; // 大範囲(タメ1.8秒)
                }
                else {
                    if (dist > 3.0f) MoveSmart(p.position, d); // 攻撃中でなければ近づく
                }
            }
            else { // 何かの大技の実行中
                if (bossAttackType == 1) { // --- 技1: 3連撃 ---
                    bossActionTimer -= GetFrameTime();
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0; bossComboStep++; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });
                        fx.SpawnEffect(spawnPos, bossTargetDir, FX_SLASH, GOLD);

                        Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                        if (Vector3Distance(hitCenter, p.position) < 2.5f) {
                            float rawDmg = 10.0f + level * 2; if (bossComboStep == 3) rawDmg *= 1.5f; // 3段目は1.5倍ダメージ
                            float dmg = fmaxf(1.0f, rawDmg - p.defense); p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg); fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_COMBO"].c_str(), (int)dmg), RED);
                        }

                        if (bossComboStep >= 3) { bossAttackType = 0; attackTimer = 1.5f; } // 3段撃ち終えたら終了
                        else {
                            bossActionTimer = 0.5f; // 次の段のタメ
                            bossTargetDir = Vector3Normalize(Vector3Subtract(p.position, position)); // 再度プレイヤーを狙う(ホーミング)
                            lastAttackDir = bossTargetDir;
                            MoveSmart(Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f)), d); // 少し踏み込む
                        }
                    }
                }
                else if (bossAttackType == 2) { // --- 技2: 前方弾幕 ---
                    bossActionTimer -= GetFrameTime();
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        Vector3 spawnPos = Vector3Add(position, { 0, 1.0f, 0 });
                        for (int i = -1; i <= 1; i++) { // 前方3方向に弾を飛ばす
                            float angle = i * 20.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                            Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                            fx.SpawnProjectile(spawnPos, dir, 12.0f, 1, false);
                        }
                        bossAttackType = 0; attackTimer = 1.5f;
                    }
                }
                else if (bossAttackType == 3) { // --- 技3: 超高速突進 ---
                    bossActionTimer -= GetFrameTime();
                    if (bossComboStep == 0) { // タメ中
                        if (bossActionTimer <= 0.0f) { bossComboStep = 1; bossActionTimer = 0.6f; animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK); }
                        else { if (GetRandomValue(0, 100) < 20) fx.SpawnEffect(Vector3Add(position, { 0,0.5f,0 }), { 0,1,0 }, FX_HIT, ORANGE); } // タメエフェクト
                    }
                    else if (bossComboStep == 1) { // 突進実行中
                        float oldSpeed = speed; speed = speed * 3.5f; // スピード3.5倍
                        bool hitWall = MoveSmart(Vector3Add(position, Vector3Scale(bossTargetDir, 10.0f)), d);
                        speed = oldSpeed;

                        fx.SpawnEffect(Vector3Add(position, { 0,1,0 }), { 0,0,0 }, FX_HIT, Fade(RED, 0.3f)); // 突進の軌跡

                        if (Vector3Distance(position, p.position) < radius + p.radius + 0.8f) { // 轢かれたらダメージ
                            float rawDmg = 15.0f + level * 2; float dmg = fmaxf(1.0f, rawDmg - p.defense); p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg); fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_DASH"].c_str(), (int)dmg), RED);
                            bossAttackType = 0; attackTimer = 2.0f;
                        }
                        if (hitWall || bossActionTimer <= 0.0f) { bossAttackType = 0; attackTimer = 2.0f; } // 壁にぶつかっても終了
                    }
                }
                else if (bossAttackType == 4) { // --- 技4: 巨大範囲攻撃(自身を中心) ---
                    bossActionTimer -= GetFrameTime();
                    if (bossActionTimer <= 0.0f) {
                        animFrameCounter = 0; AudioManager::PlaySE(SE_ENEMY_ATTACK);
                        for (int i = 0; i < 12; i++) { // 円形にエフェクトを発生
                            float angle = i * 30.0f * DEG2RAD; Vector3 dir = { cosf(angle), 0.0f, sinf(angle) };
                            fx.SpawnEffect(Vector3Add(position, { 0, 0.5f, 0 }), dir, FX_SMASH, PURPLE);
                        }
                        float aoeRadius = 7.0f; // 半径7マス分の大ダメージ
                        if (Vector3Distance(position, p.position) < aoeRadius) {
                            float rawDmg = 20.0f + level * 2; float dmg = fmaxf(1.0f, rawDmg - p.defense); p.hp -= dmg;
                            fx.SpawnDamageText(p.position, (int)dmg); fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                            UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_BOSS_AOE"].c_str(), (int)dmg), RED);
                        }
                        bossAttackType = 0; attackTimer = 2.5f;
                    }
                    else { if (GetRandomValue(0, 100) < 15) fx.SpawnEffect(Vector3Add(position, { 0,0.5f,0 }), { 0,1,0 }, FX_HIT, MAGENTA); } // タメエフェクト
                }
            }
        }
        else { // ボスがプレイヤーを見失った場合(巡回)
            if (Vector3Distance(position, patrolTarget) < 1.2f) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            bool hitWall = MoveSmart(patrolTarget, d);
            if (hitWall) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++; else stuckCount = 0;
            if (stuckCount > 60) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
        }
    }
    // --- 通常の敵のAIループ ---
    else
    {
        if (dist < attackRange && canSee && dist < effectiveDetect) state = STATE_ATTACK;
        else if (dist < effectiveDetect && canSee) state = STATE_CHASE;
        else state = STATE_PATROL;

        if (state == STATE_CHASE || state == STATE_ATTACK) {
            stuckCount = 0;
            if (eType != E_TRAP) { if (dist > attackRange * 0.7f) MoveSmart(p.position, d); } // 近接型は攻撃距離まで近づく

            if (dist < attackRange && attackTimer <= 0) {
                lastAttackDir = Vector3Normalize(Vector3Subtract(p.position, position));
                Vector3 spawnPos = Vector3Add(position, { 0, 0.8f, 0 });
                AudioManager::PlaySE(SE_ENEMY_ATTACK);

                if (eType == E_ARCHER) { fx.SpawnProjectile(spawnPos, lastAttackDir, 18.0f, 0, false); attackTimer = 1.8f; } // 弓(速い弾)
                else if (eType == E_MAGE) { fx.SpawnProjectile(spawnPos, lastAttackDir, 8.0f, 1, false); attackTimer = 2.2f; } // 魔法(遅くて大きい弾)
                else if (eType == E_TRAP) { fx.SpawnProjectile(spawnPos, lastAttackDir, 12.0f, 0, false); attackTimer = 2.5f; } // 罠
                else { // 近接攻撃(剣、槍、斧)
                    EffectType effectType = FX_SLASH;
                    if (eType == E_SPEAR) effectType = FX_THRUST;
                    else if (eType == E_AXE) effectType = FX_SMASH;

                    fx.SpawnEffect(spawnPos, lastAttackDir, effectType, GOLD);

                    float rawDmg = 10.0f + level * 2;
                    float dmg = fmaxf(1.0f, rawDmg - p.defense);
                    p.hp -= dmg;

                    fx.SpawnDamageText(p.position, (int)dmg); fx.SpawnEffect(p.position, { 0,0,0 }, FX_HIT, RED);
                    UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_DMG_TAKEN"].c_str(), data.name.c_str(), (int)dmg), RED);

                    attackTimer = 1.5f;
                }
            }
        }
        else { // 巡回
            if (Vector3Distance(position, patrolTarget) < 1.2f) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            bool hitWall = MoveSmart(patrolTarget, d);
            if (hitWall) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
            if (Vector3Distance(position, lastPos) < 0.02f) stuckCount++; else stuckCount = 0;
            if (stuckCount > 60) { patrolTarget = d.GetRandomFloorPos(); stuckCount = 0; }
        }
    }
    lastPos = position;
}

// 敵モデルの描画と、ボスの「予兆線(Telegraph)」の描画
void Enemy::Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos) {
    int currentAnimIndex = -1;
    bool hasModel = false;

    std::string key = data.modelName;
    if (!key.empty() && DataManager::loadedModels.count(key) > 0) {
        hasModel = true; GameModel& gm = DataManager::loadedModels[key]; gm.model.transform = MatrixIdentity();

        // 状態に合わせて再生するアニメーションの番号を決める
        int animIndex = 2; // デフォルト(待機など)
        if (isDying) animIndex = 1; // 死亡
        else if (state == STATE_ATTACK) {
            if (isBoss) {
                if (bossAttackType == 1) animIndex = 0; // 3連撃(近接モーション)
                else if (bossAttackType == 2 || bossAttackType == 4) animIndex = 2; // 魔法や範囲攻撃は待機モーションのまま
                else if (bossAttackType == 3) animIndex = (bossComboStep == 1) ? 3 : 2; // 突進中は走りモーション(3)
                else { if (attackTimer > 1.0f) animIndex = 2; else animIndex = 3; }
            }
            else animIndex = 0; // 一般敵の攻撃モーション
        }
        else if (state == STATE_CHASE || state == STATE_PATROL) animIndex = 3; // 走りモーション

        if (animIndex >= gm.animCount) animIndex = 0;
        currentAnimIndex = animIndex;

        int frame = animFrameCounter; ModelAnimation anim = gm.anims[currentAnimIndex];
        if (isDying) { if (frame >= anim.frameCount - 1) frame = anim.frameCount - 1; } // 死亡時は最後のフレームで止める
        else { frame = frame % anim.frameCount; }

        if (gm.animCount > 0) UpdateModelAnimation(gm.model, anim, frame);

        // --- 敵の向く角度を決定 ---
        float rotationAngle = 0.0f; Vector3 targetDir = { 0, 0, 1 };
        if (!isDying) {
            if (isBoss) {
                if (bossAttackType != 0) targetDir = lastAttackDir; // 大技中は攻撃方向を向いたまま
                else if (state == STATE_ATTACK || state == STATE_CHASE) targetDir = Vector3Subtract(playerPos, position);
                else targetDir = Vector3Subtract(patrolTarget, position);
            }
            else {
                if (state == STATE_CHASE || state == STATE_ATTACK) targetDir = Vector3Subtract(playerPos, position);
                else targetDir = Vector3Subtract(patrolTarget, position);
            }
            if (Vector3Length(targetDir) > 0.01f) rotationAngle = atan2f(targetDir.x, targetDir.z) * RAD2DEG;
        }

        float scale = 0.01f;
        Vector3 drawPos = { position.x, position.y - 0.4f, position.z }; // プレイヤーと同じく少し下げて接地させる

        Matrix matRotX = MatrixRotateX(-90.0f * DEG2RAD);
        Matrix matRotY = MatrixRotateY(rotationAngle * DEG2RAD);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotX);
        gm.model.transform = MatrixMultiply(gm.model.transform, matRotY);

        DrawModel(gm.model, drawPos, scale, WHITE);

        // --- 敵の武器の描画 ---
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
                if (wGm.animCount > 0) UpdateModelAnimation(wGm.model, wGm.anims[0], frame % wGm.anims[0].frameCount);
                DrawModel(wGm.model, { 0,0,0 }, 1.0f, WHITE);
                wGm.model.transform = MatrixIdentity();
            }
        }
        gm.model.transform = MatrixIdentity();
    }
    else {
        // 代替モデル(色付きの箱)の場合も接地させる
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

    // --- ボスの予兆線(Telegraph)の描画 ---
    // Raylibの3Dポリゴン関数を使い、攻撃範囲を赤く警告表示する
    if (!isDying && isBoss && bossAttackType != 0) {
        float maxTime = 1.0f;
        if (bossAttackType == 1) maxTime = 0.5f;
        else if (bossAttackType == 2) maxTime = 0.8f;
        else if (bossAttackType == 3) maxTime = 1.0f;
        else if (bossAttackType == 4) maxTime = 1.8f;

        bool showTelegraph = true;
        if (bossAttackType == 3 && bossComboStep == 1) showTelegraph = false; // 突進中は消す

        if (showTelegraph) {
            float progress = 1.0f - (bossActionTimer / maxTime); // 0.0(タメ開始) -> 1.0(発射直前)
            if (progress < 0.0f) progress = 0.0f; if (progress > 1.0f) progress = 1.0f;

            Color warnColor = Fade(RED, progress * 0.4f); // タメるほど濃くなる
            Color warnLineColor = Fade(MAROON, progress * 0.8f);

            if (bossAttackType == 1) { // 目の前の円形範囲
                Vector3 hitCenter = Vector3Add(position, Vector3Scale(bossTargetDir, 2.0f));
                DrawCylinder({ hitCenter.x, 0.05f, hitCenter.z }, 2.5f, 2.5f, 0.05f, 32, warnColor);
                DrawCylinderWires({ hitCenter.x, 0.05f, hitCenter.z }, 2.5f, 2.5f, 0.05f, 32, warnLineColor);
            }
            else if (bossAttackType == 2) { // 3方向の帯(扇状)
                for (int i = -1; i <= 1; i++) {
                    float angle = i * 20.0f * DEG2RAD; float c = cosf(angle), s = sinf(angle);
                    Vector3 dir = { bossTargetDir.x * c - bossTargetDir.z * s, 0.0f, bossTargetDir.x * s + bossTargetDir.z * c };
                    float len = 20.0f; float w = 1.5f; Vector3 side = { -dir.z, 0.0f, dir.x };
                    Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                    Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, len), Vector3Scale(side, w / 2.0f)));
                    Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, len), Vector3Scale(side, -w / 2.0f)));
                    Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                    p1.y = p2.y = p3.y = p4.y = 0.05f;

                    DrawTriangle3D(p1, p4, p3, warnColor); DrawTriangle3D(p1, p3, p2, warnColor);
                    DrawTriangle3D(p3, p4, p1, warnColor); DrawTriangle3D(p2, p3, p1, warnColor);
                    DrawLine3D(p1, p2, warnLineColor); DrawLine3D(p2, p3, warnLineColor);
                    DrawLine3D(p3, p4, warnLineColor); DrawLine3D(p4, p1, warnLineColor);
                }
            }
            else if (bossAttackType == 3) { // 突進ルートを示す太い帯
                float len = 10.0f + 2.75f; float w = 5.5f; Vector3 dir = Vector3Normalize(bossTargetDir);
                Vector3 side = { -dir.z, 0.0f, dir.x };
                Vector3 p1 = Vector3Add(position, Vector3Scale(side, w / 2.0f));
                Vector3 p2 = Vector3Add(position, Vector3Add(Vector3Scale(dir, len), Vector3Scale(side, w / 2.0f)));
                Vector3 p3 = Vector3Add(position, Vector3Add(Vector3Scale(dir, len), Vector3Scale(side, -w / 2.0f)));
                Vector3 p4 = Vector3Add(position, Vector3Scale(side, -w / 2.0f));
                p1.y = p2.y = p3.y = p4.y = 0.05f;

                DrawTriangle3D(p1, p4, p3, warnColor); DrawTriangle3D(p1, p3, p2, warnColor);
                DrawTriangle3D(p3, p4, p1, warnColor); DrawTriangle3D(p2, p3, p1, warnColor);
                DrawLine3D(p1, p2, warnLineColor); DrawLine3D(p2, p3, warnLineColor);
                DrawLine3D(p3, p4, warnLineColor); DrawLine3D(p4, p1, warnLineColor);
            }
            else if (bossAttackType == 4) { // 自身を中心とした巨大な円
                DrawCylinder({ position.x, 0.05f, position.z }, 7.0f, 7.0f, 0.05f, 32, warnColor);
                DrawCylinderWires({ position.x, 0.05f, position.z }, 7.0f, 7.0f, 0.05f, 32, warnLineColor);
            }
        }
    }
}