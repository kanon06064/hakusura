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
    if (p.pos.y < 0.5f) p.pos.y = 1.2f;
    p.vel = Vector3Scale(Vector3Normalize(dir), speed);
    p.radius = (type == 1) ? 0.6f : 0.2f;
    p.active = true;
    p.type = type;
    p.isPlayer = isPlayer;
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
    eff.scale = scale; // 射程スケール
    effects.push_back(eff);
}

void EffectManager::SpawnDamageText(Vector3 pos, int dmg, bool isCrit) {
    damageTexts.push_back({ Vector3Add(pos, {0, 1.5f, 0}), dmg, isCrit ? 1.2f : 1.0f, isCrit });
}

void EffectManager::Update(float dt, Dungeon& d) {
    // 画面揺れタイマー更新
    if (shakeTimer > 0.0f) {
        shakeTimer -= dt;
        if (shakeTimer <= 0.0f) {
            shakeTimer = 0.0f;
            shakeIntensity = 0.0f;
        }
    }

    // 画面赤フラッシュタイマー更新
    if (damageFlashTimer > 0.0f) {
        damageFlashTimer -= dt;
        if (damageFlashTimer < 0.0f) damageFlashTimer = 0.0f;
    }

    // 弾の移動処理と壁との衝突判定
    for (auto& p : projectiles) {
        if (!p.active) continue;
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        if (d.IsWall(p.pos.x, p.pos.z)) p.active = false;
        if (p.pos.x < 0 || p.pos.x > MAX_MAP_WIDTH * TILE_SIZE ||
            p.pos.z < 0 || p.pos.z > MAX_MAP_HEIGHT * TILE_SIZE) {
            p.active = false;
        }
    }

    // エフェクトの寿命更新
    for (auto& e : effects) e.life -= dt;
    for (auto& t : damageTexts) {
        t.life -= dt;
        t.pos.y += 0.5f * dt;
    }

    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.active; }), projectiles.end());
    effects.erase(std::remove_if(effects.begin(), effects.end(), [](const VisualEffect& e) { return e.life <= 0; }), effects.end());
    damageTexts.erase(std::remove_if(damageTexts.begin(), damageTexts.end(), [](const DamageText& t) { return t.life <= 0; }), damageTexts.end());
}

Vector3 EffectManager::GetShakeOffset() const {
    if (shakeTimer <= 0.0f || shakeIntensity  <= 0.0f) {
        return { 0.0f, 0.0f, 0.0f };
    }
    float currentMag = shakeIntensity;
    float rx = ((float)GetRandomValue(-100, 100) / 100.0f) * currentMag;
    float ry = ((float)GetRandomValue(-100, 100) / 100.0f) * currentMag;
    float rz = ((float)GetRandomValue(-100, 100) / 100.0f) * currentMag;
    return { rx, ry, rz };
}

void EffectManager::CheckProjectileCollisions(std::vector<Enemy>& enemies, Player& p, Dungeon& d) {
    for (auto& proj : projectiles) {
        if (!proj.active) continue;

        if (proj.isPlayer) {
            // プレイヤーの弾（魔法等）の敵命中判定
            for (auto& e : enemies) {
                if (Vector3Distance(proj.pos, e.position) < (proj.radius + e.radius + 0.3f)) {
                    // 15%でクリティカル
                    bool isCrit = (GetRandomValue(1, 100) <= 15);

                    float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[p.activeSlot]);
                    int weaponElem = p.equippedData[p.activeSlot].element;
                    float elemMulti = Player::GetElementMultiplier(weaponElem, e.data.element);

                    int dmg = (int)((p.attackPower + totalBonus) * elemMulti) + GetRandomValue(0, 5);
                    if (isCrit) dmg = (int)((float)dmg * 1.75f);

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
            // 敵の弾のプレイヤー命中判定
            if (Vector3Distance(proj.pos, p.position) < (proj.radius + p.radius + 0.2f)) {
                float rawDmg = 12.0f;
                float defDmg = fmaxf(1.0f, rawDmg - p.defense);

                // 弾属性耐性カット（type 1: 深淵、0: 無属性）
                int projElem = (proj.type == 1) ? ELEM_ABYSS : ELEM_NONE;
                float resist = p.GetPlayerElementResistance(projElem);
                float finalDmg = fmaxf(1.0f, defDmg * (1.0f - resist));

                p.hp -= finalDmg;

                SpawnDamageText(p.position, (int)finalDmg);
                SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, RED);
                ShakeScreen(0.15f, 0.3f);
                TriggerDamageFlash(0.25f);

                proj.active = false;
            }
        }
    }
}

void EffectManager::Draw() {
    // 1. プロジェクタイル（弾）
    for (const auto& p : projectiles) {
        Color c = (p.type == 0) ? YELLOW : PURPLE;
        if (!p.isPlayer) c = RED;

        DrawSphere(p.pos, p.radius * 0.7f, WHITE);
        DrawSphere(p.pos, p.radius, c);
        DrawSphere(p.pos, p.radius * 1.6f, Fade(c, 0.35f));

        Vector3 tail = Vector3Subtract(p.pos, Vector3Scale(Vector3Normalize(p.vel), 0.7f));
        DrawLine3D(p.pos, tail, Fade(WHITE, 0.8f));
    }

    // 2. 近接・スキルエフェクト
    for (const auto& e : effects) {
        float ratio = e.life / e.maxLife;
        Color c = Fade(e.color, ratio);

        // 【剣】FX_SLASH：前方に丸く膨らむ王道アーク、左から右へ薙ぎ払う三日月ブレード
        if (e.type == FX_SLASH) {
            float baseAngle = atan2f(e.dir.z, e.dir.x);
            Vector3 center = e.pos;

            float progress = 1.0f - (e.life / e.maxLife);
            float pLead = progress * 1.5f;   if (pLead > 1.0f) pLead = 1.0f;
            float pTrail = (progress - 0.22f) * 1.5f; if (pTrail < 0.0f) pTrail = 0.0f;

            if (pLead > pTrail) {
                float totalArc = 105.0f * DEG2RAD; // 角度幅105度
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
                    float innerR = 0.6f;

                    // 両端が針のように鋭く尖り、中央がふっくら太い三日月プロファイル
                    float thick1 = (maxR - innerR) * sinf(t1 * 3.14159265f);
                    float thick2 = (maxR - innerR) * sinf(t2 * 3.14159265f);

                    float outerR1 = innerR + thick1;
                    float outerR2 = innerR + thick2;

                    Vector3 pIn1 = { center.x + cosf(a1) * innerR,  center.y, center.z + sinf(a1) * innerR };
                    Vector3 pOut1 = { center.x + cosf(a1) * outerR1, center.y, center.z + sinf(a1) * outerR1 };
                    Vector3 pIn2 = { center.x + cosf(a2) * innerR,  center.y, center.z + sinf(a2) * innerR };
                    Vector3 pOut2 = { center.x + cosf(a2) * outerR2, center.y, center.z + sinf(a2) * outerR2 };

                    float alpha1 = ratio * (t1 * 0.8f);
                    float alpha2 = ratio * (t2 * 0.8f);

                    DrawTriangle3D(pIn1, pOut1, pOut2, Fade(e.color, alpha2));
                    DrawTriangle3D(pIn1, pOut2, pIn2, Fade(e.color, alpha1));
                    DrawTriangle3D(pIn1, pOut2, pOut1, Fade(e.color, alpha2));
                    DrawTriangle3D(pIn1, pIn2, pOut2, Fade(e.color, alpha1));

                    DrawLine3D(pOut1, pOut2, Fade(WHITE, alpha2 * 0.9f));
                }
            }
        }
        // 【槍】FX_THRUST：前方を鋭く貫く光の槍
        else if (e.type == FX_THRUST) {
            Vector3 start = e.pos;
            Vector3 end = Vector3Add(e.pos, Vector3Scale(e.dir, e.scale));

            DrawCylinderEx(start, end, 0.12f * ratio, 0.04f * ratio, 8, Fade(WHITE, ratio * 0.9f));
            DrawCylinderEx(start, end, 0.35f * ratio, 0.12f * ratio, 8, Fade(e.color, ratio * 0.45f));

            DrawSphere(end, 0.3f * ratio, WHITE);
            DrawSphere(end, 0.5f * ratio, Fade(e.color, ratio * 0.4f));
        }
        // 【斧】FX_SMASH：地面を叩き割るように広がる円形衝撃波
        else if (e.type == FX_SMASH) {
            Vector3 impactPos = Vector3Add(e.pos, Vector3Scale(e.dir, e.scale * 0.6f));
            impactPos.y = 0.08f;

            float expand = (1.0f - ratio);
            float currentRadius = (e.scale * 0.4f) + expand * (e.scale * 0.6f);

            DrawCylinder(impactPos, currentRadius, currentRadius, 0.03f, 28, Fade(e.color, ratio * 0.45f));
            DrawCylinderWires(impactPos, currentRadius, currentRadius, 0.04f, 28, Fade(GOLD, ratio * 0.9f));
            DrawCylinderWires(impactPos, currentRadius * 0.7f, currentRadius * 0.7f, 0.04f, 28, Fade(WHITE, ratio * 0.6f));
        }
        // 【ヒット時】FX_HIT：光のスパーク
        else if (e.type == FX_HIT) {
            DrawSphere(e.pos, 0.2f * ratio, WHITE);
            DrawSphere(e.pos, 0.4f * ratio, Fade(e.color, ratio * 0.5f));
        }
    }
}

void EffectManager::Draw2D(Font font, Camera3D cam) {
    for (const auto& dt : damageTexts) {
        Vector2 s = GetWorldToScreen(dt.pos, cam);
        if (s.x < 0 || s.y < 0 || s.x > GetScreenWidth() || s.y > GetScreenHeight()) continue;

        if (dt.amount == 999) {
            DrawTextEx(font, "LEVEL UP!!", { s.x - 50, s.y - 30 }, 28, 1, YELLOW);
        }
        // クリティカル時の特大ゴールド表示
        else if (dt.isCrit) {
            std::string critTxt = TextFormat("CRIT! %d", dt.amount);
            Vector2 tSize = MeasureTextEx(font, critTxt.c_str(), 34, 1);
            Vector2 drawPos = { s.x - tSize.x / 2.0f, s.y - 20.0f };

            DrawTextEx(font, critTxt.c_str(), { drawPos.x + 2, drawPos.y + 2 }, 34, 1, Fade(BLACK, dt.life));
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