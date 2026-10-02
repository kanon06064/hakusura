#include "EffectManager.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "Player.h"
#include "raymath.h"
#include <math.h>
#include <algorithm>

void EffectManager::SpawnProjectile(Vector3 pos, Vector3 dir, float speed, int type, bool isPlayer) {
    Projectile p;
    p.pos = pos;
    if (p.pos.y < 0.5f) p.pos.y = 1.2f; // 地面スレスレにならないよう高さを調整
    p.vel = Vector3Scale(Vector3Normalize(dir), speed); // 速度ベクトルを計算
    p.radius = (type == 1) ? 0.6f : 0.2f; // 魔法の種類によって当たり判定のサイズを変える
    p.active = true;
    p.type = type;
    p.isPlayer = isPlayer; // プレイヤーが撃った弾か、敵が撃った弾か
    projectiles.push_back(p);
}

void EffectManager::SpawnEffect(Vector3 pos, Vector3 dir, EffectType type, Color col, float scale) {
    VisualEffect eff;
    eff.pos = pos;
    eff.dir = dir;
    eff.type = type;
    eff.color = col;
    eff.life = 0.3f;
    eff.maxLife = 0.3f;
    eff.scale = scale; 
    effects.push_back(eff);
}

void EffectManager::SpawnDamageText(Vector3 pos, int dmg, bool isCrit) {
    // クリティカル時は表示時間を少し長め（1.2秒）にする
    damageTexts.push_back({ Vector3Add(pos, {0, 1.5f, 0}), dmg, isCrit ? 1.2f : 1.0f, isCrit });
}

void EffectManager::Update(float dt, Dungeon& d) {

    if (shakeTimer > 0.0f) {
        shakeTimer -= dt;
        if (shakeTimer <= 0.0f) {
            shakeTimer = 0.0f;
            shakeIntensity = 0.0f;
        }
    }

    if (damageFlashTimer > 0.0f) {
        damageFlashTimer -= dt;
        if (damageFlashTimer < 0.0f) damageFlashTimer = 0.0f;
    }

    // 弾の移動処理と壁との衝突判定
    for (auto& p : projectiles) {
        if (!p.active) continue;
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        // 壁にぶつかるか、マップ外に出たら消滅させる
        if (d.IsWall(p.pos.x, p.pos.z)) p.active = false;
        if (p.pos.x < 0 || p.pos.x > MAX_MAP_WIDTH * TILE_SIZE ||
            p.pos.z < 0 || p.pos.z > MAX_MAP_HEIGHT * TILE_SIZE) {
            p.active = false;
        }
    }

    // エフェクトの寿命を減らす
    for (auto& e : effects) e.life -= dt;
    for (auto& t : damageTexts) {
        t.life -= dt;
        t.pos.y += 0.5f * dt; // ダメージテキストはゆっくり上に登っていく
    }

    // 寿命が尽きた(active=falseになった)ものをリストから削除する
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.active; }), projectiles.end());
    effects.erase(std::remove_if(effects.begin(), effects.end(), [](const VisualEffect& e) { return e.life <= 0; }), effects.end());
    damageTexts.erase(std::remove_if(damageTexts.begin(), damageTexts.end(), [](const DamageText& t) { return t.life <= 0; }), damageTexts.end());


    }

void EffectManager::CheckProjectileCollisions(std::vector<Enemy>& enemies, Player& p, Dungeon& d) {
    for (auto& proj : projectiles) {
        if (!proj.active) continue;

        if (proj.isPlayer) {
            // プレイヤーが撃った弾が敵に当たったか判定
            for (auto& e : enemies) {
                if (Vector3Distance(proj.pos, e.position) < (proj.radius + e.radius + 0.3f)) {
                    // ★ 魔法弾も5%の確率でクリティカル
                    bool isCrit = (GetRandomValue(1, 100) <= 5);

                    float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[p.activeSlot]);
                    int dmg = (int)(p.attackPower + totalBonus) + GetRandomValue(0, 5);
                    if (isCrit) dmg = (int)(dmg * 1.75f);

                    e.hp -= (float)dmg;
                    e.hudTimer = 5.0f;
                    e.ApplyKnockback(Vector3Normalize(proj.vel), isCrit ? 1.0f : 0.5f, d);

                    SpawnDamageText(e.position, dmg, isCrit);
                    SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, isCrit ? GOLD : PURPLE);

                    if (isCrit) {
                        TriggerHitStop(0.06f);
                        ShakeScreen(0.15f, 0.3f);
                    }

                    proj.active = false;
                    break;
                }
            }
        }
        else {
            // 敵が撃った弾がプレイヤーに当たったか判定
            if (Vector3Distance(proj.pos, p.position) < (proj.radius + p.radius + 0.2f)) {
                float rawDmg = 12.0f;
                // 防御力によるダメージ減衰 (最低でも1ダメージは受ける)
                float dmg = fmaxf(1.0f, rawDmg - p.defense);
                p.hp -= dmg;

                SpawnDamageText(p.position, (int)dmg);
                SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, RED);
                ShakeScreen(0.15f, 0.3f);
                TriggerDamageFlash(0.25f);

                proj.active = false;
            }
        }
    }
}

void EffectManager::Draw() {
    // =========================================================================
    // 1. 魔法弾・矢（プロジェクタイル：杖や敵の弾）
    // =========================================================================
    for (const auto& p : projectiles) {
        Color c = (p.type == 0) ? YELLOW : PURPLE;
        if (!p.isPlayer) c = RED;

        // コア（白い芯）＋ オーラ（半透明の外殻）で発光感を演出
        DrawSphere(p.pos, p.radius * 0.7f, WHITE);
        DrawSphere(p.pos, p.radius, c);
        DrawSphere(p.pos, p.radius * 1.6f, Fade(c, 0.35f));

        // 飛翔軌跡（残光ライン）
        Vector3 tail = Vector3Subtract(p.pos, Vector3Scale(Vector3Normalize(p.vel), 0.7f));
        DrawLine3D(p.pos, tail, Fade(WHITE, 0.8f));
    }

    // =========================================================================
    // 2. 近接・スキルエフェクト（武器種ごとの専用描画）
    // =========================================================================
    for (const auto& e : effects) {
        float ratio = e.life / e.maxLife; // 1.0 (発生直後) 〜 0.0 (消滅)
        Color c = Fade(e.color, ratio);

        // ---------------------------------------------------------------------
        // 【剣】FX_SLASH：弧を描いて薙ぎ払う三日月型の光の斬撃帯
        // ---------------------------------------------------------------------
        if (e.type == FX_SLASH) {
            float baseAngle = atan2f(e.dir.z, e.dir.x);
            Vector3 center = e.pos;

            float progress = 1.0f - (e.life / e.maxLife);
            float pLead = progress * 1.5f;   if (pLead > 1.0f) pLead = 1.0f;
            float pTrail = (progress - 0.22f) * 1.5f; if (pTrail < 0.0f) pTrail = 0.0f;

            if (pLead > pTrail) {
                // ★ 角度幅を130度から105度に短縮してキレを出す
                float totalArc = 105.0f * DEG2RAD;
                float leftAngle = baseAngle - (totalArc * 0.5f);
                int segments = 16;

                for (int i = 0; i < segments; i++) {
                    float t1 = (float)i / segments;
                    float t2 = (float)(i + 1) / segments;

                    float segP1 = pTrail + (pLead - pTrail) * t1;
                    float segP2 = pTrail + (pLead - pTrail) * t2;

                    float a1 = leftAngle + (totalArc * segP1);
                    float a2 = leftAngle + (totalArc * segP2);

                    float maxR = e.scale;
                    float innerR = 0.6f; // ★ 手元の余白を少し詰める

                    float thick1 = (maxR - innerR) * sinf(t1 * 3.14159265f);
                    float thick2 = (maxR - innerR) * sinf(t2 * 3.14159265f);

                    float outerR1 = innerR + thick1;
                    float outerR2 = innerR + thick2;

                    Vector3 pIn1 = { center.x + cosf(a1) * innerR,  center.y, center.z + sinf(a1) * innerR };
                    Vector3 pOut1 = { center.x + cosf(a1) * outerR1, center.y, center.z + sinf(a1) * outerR1 };
                    Vector3 pIn2 = { center.x + cosf(a2) * innerR,  center.y, center.z + sinf(a2) * innerR };
                    Vector3 pOut2 = { center.x + cosf(a2) * outerR2, center.y, center.z + sinf(a2) * outerR2 };

                    // 先端（t=1.0）側ほど明るく、末尾（t=0.0）側ほどフェードアウト
                    float alpha1 = ratio * (t1 * 0.8f);
                    float alpha2 = ratio * (t2 * 0.8f);

                    // 両面描画（見下ろしカメラでしっかり視認）
                    DrawTriangle3D(pIn1, pOut1, pOut2, Fade(e.color, alpha2));
                    DrawTriangle3D(pIn1, pOut2, pIn2, Fade(e.color, alpha1));
                    DrawTriangle3D(pIn1, pOut2, pOut1, Fade(e.color, alpha2));
                    DrawTriangle3D(pIn1, pIn2, pOut2, Fade(e.color, alpha1));

                    // 刃先のエッジライン（外周の白い発光線）
                    DrawLine3D(pOut1, pOut2, Fade(WHITE, alpha2 * 0.9f));
                }
            }
        }
        // -------------------------------------------------------------
        // 【槍】FX_THRUST：指定した射程(e.scale)までぴったり光線が伸びる
        // -------------------------------------------------------------
        else if (e.type == FX_THRUST) {
            Vector3 start = e.pos;
            // ★ e.scale の長さまで真っ直ぐ伸びる
            Vector3 end = Vector3Add(e.pos, Vector3Scale(e.dir, e.scale));

            DrawCylinderEx(start, end, 0.12f * ratio, 0.04f * ratio, 8, Fade(WHITE, ratio * 0.9f));
            DrawCylinderEx(start, end, 0.35f * ratio, 0.12f * ratio, 8, Fade(e.color, ratio * 0.45f));

            DrawSphere(end, 0.3f * ratio, WHITE);
            DrawSphere(end, 0.5f * ratio, Fade(e.color, ratio * 0.4f));
        }
        // -------------------------------------------------------------
        // 【斧】FX_SMASH：前方の着弾地点と衝撃波の半径を e.scale に連動
        // -------------------------------------------------------------
        else if (e.type == FX_SMASH) {
            // 前方への振り下ろし距離
            Vector3 impactPos = Vector3Add(e.pos, Vector3Scale(e.dir, e.scale * 0.6f));
            impactPos.y = 0.08f;

            float expand = (1.0f - ratio);
            // ★ 衝撃波の最大広がりを e.scale に合わせる
            float currentRadius = (e.scale * 0.4f) + expand * (e.scale * 0.6f);

            DrawCylinder(impactPos, currentRadius, currentRadius, 0.03f, 28, Fade(e.color, ratio * 0.45f));
            DrawCylinderWires(impactPos, currentRadius, currentRadius, 0.04f, 28, Fade(GOLD, ratio * 0.9f));
            DrawCylinderWires(impactPos, currentRadius * 0.7f, currentRadius * 0.7f, 0.04f, 28, Fade(WHITE, ratio * 0.6f));
        }
        // ---------------------------------------------------------------------
        // 【ヒット時】FX_HIT：命中瞬間の光のスパーク
        // ---------------------------------------------------------------------
        else if (e.type == FX_HIT) {
            DrawSphere(e.pos, 0.2f * ratio, WHITE);
            DrawSphere(e.pos, 0.4f * ratio, Fade(e.color, ratio * 0.5f));
        }
    }
}

void EffectManager::Draw2D(Font font, Camera3D cam) {
    // ダメージテキストは3D空間の座標をスクリーンの2D座標に変換して描画する
    for (const auto& dt : damageTexts) {
        Vector2 s = GetWorldToScreen(dt.pos, cam);
        // 画面外なら描画しない
        if (s.x < 0 || s.y < 0 || s.x > GetScreenWidth() || s.y > GetScreenHeight()) continue;

        if (dt.amount == 999) { // 999はレベルアップ時の特殊フラグ
            DrawTextEx(font, "LEVEL UP!!", { s.x - 50, s.y - 30 }, 28, 1, YELLOW);
        }
        else if (dt.isCrit) {
            std::string critTxt = TextFormat("CRIT! %d", dt.amount);
            Vector2 tSize = MeasureTextEx(font, critTxt.c_str(), 34, 1);
            Vector2 drawPos = { s.x - tSize.x / 2.0f, s.y - 20.0f };

            // 黒の縁取り（影）
            DrawTextEx(font, critTxt.c_str(), { drawPos.x + 2, drawPos.y + 2 }, 34, 1, Fade(BLACK, dt.life));
            // 鮮やかな金色/黄色で発光
            DrawTextEx(font, critTxt.c_str(), drawPos, 34, 1, Fade(GOLD, dt.life));
        }
        else {
            Color c = ORANGE;
            if (dt.amount > 25) c = RED;
            std::string dmgTxt = TextFormat("%d", dt.amount);
            DrawTextEx(font, dmgTxt.c_str(), { s.x - 10, s.y }, 22, 1, Fade(c, dt.life));
        }
    }
}

Vector3 EffectManager::GetShakeOffset() const {
    if (shakeTimer <= 0.0f || shakeIntensity <= 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    // 時間経過で揺れを徐々に小さくする
    float currentMag = shakeIntensity;
    float rx = ((float)GetRandomValue(-100, 100) / 100.0f) * currentMag;
    float ry = ((float)GetRandomValue(-100, 100) / 100.0f) * currentMag;
    float rz = ((float)GetRandomValue(-100, 100) / 100.0f) * currentMag;
    return { rx, ry, rz };
}