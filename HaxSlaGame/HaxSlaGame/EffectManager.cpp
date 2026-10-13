#include "EffectManager.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "Player.h"
#include "raymath.h"
#include <math.h>
#include <algorithm>
#include "DataManager.h"
#include "UI.h"

// ★ 攻撃力 (damage) と属性 (element) を弾丸に受け渡す
void EffectManager::SpawnProjectile(Vector3 pos, Vector3 dir, float speed, int type, bool isPlayer, float damage, int element) {
    Projectile p;
    p.pos = pos;
    if (p.pos.y < 0.4f || p.pos.y > 1.0f) p.pos.y = 0.75f;
    p.vel = Vector3Scale(Vector3Normalize(dir), speed);
    p.radius = (type == 1) ? 0.55f : 0.28f;
    p.active = true;
    p.type = type;
    p.isPlayer = isPlayer;
    p.damage = damage;
    p.element = element;
    projectiles.push_back(p);
}

void EffectManager::SpawnEffect(Vector3 pos, Vector3 dir, EffectType type, Color col, float scale) {
    VisualEffect eff;
    eff.pos = pos;
    eff.dir = dir;
    eff.type = type;
    eff.color = col;
    eff.life = (type == FX_EXPLOSION) ? 0.6f : 0.3f;
    eff.maxLife = eff.life;
    eff.scale = scale;
    effects.push_back(eff);
}

void EffectManager::SpawnDamageText(Vector3 pos, int dmg, bool isCrit) {
    damageTexts.push_back({ Vector3Add(pos, {0, 1.5f, 0}), dmg, isCrit ? 1.2f : 1.0f, isCrit });
}

void EffectManager::SpawnHazardArea(Vector3 pos, float radius, float duration, float dps, Color col, int element) {
    HazardArea hz;
    hz.pos = pos;
    hz.pos.y = 0.05f;
    hz.radius = radius;
    hz.duration = duration;
    hz.maxDuration = duration;
    hz.tickTimer = 0.5f;
    hz.dps = dps;
    hz.color = col;
    hz.element = element;
    hz.active = true;
    hazardAreas.push_back(hz);
}

void EffectManager::TriggerBossDefeated() {
    slowMotionTimer = 1.2f;
    slowMotionFactor = 0.25f;
    victoryBannerTimer = 4.5f;
    ShakeScreen(0.8f, 0.9f);
    TriggerHitStop(0.18f);
    TriggerDamageFlash(0.35f);
}

void EffectManager::Update(float dt, Dungeon& d) {
    if (slowMotionTimer > 0.0f) {
        slowMotionTimer -= dt;
        if (slowMotionTimer <= 0.0f) {
            slowMotionTimer = 0.0f;
            slowMotionFactor = 1.0f;
        }
    }
    else {
        slowMotionFactor = 1.0f;
    }

    if (victoryBannerTimer > 0.0f) {
        victoryBannerTimer -= dt;
        if (victoryBannerTimer < 0.0f) victoryBannerTimer = 0.0f;
    }

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

    for (auto& p : projectiles) {
        if (!p.active) continue;
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        if (d.IsWall(p.pos.x, p.pos.z)) p.active = false;
        if (p.pos.x < 0 || p.pos.x > MAX_MAP_WIDTH * TILE_SIZE ||
            p.pos.z < 0 || p.pos.z > MAX_MAP_HEIGHT * TILE_SIZE) {
            p.active = false;
        }
    }

    for (auto& hz : hazardAreas) {
        if (!hz.active) continue;
        hz.duration -= dt;
        if (hz.duration <= 0.0f) hz.active = false;
    }

    for (auto& e : effects) e.life -= dt;
    for (auto& t : damageTexts) {
        t.life -= dt;
        t.pos.y += 0.5f * dt;
    }

    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.active; }), projectiles.end());
    hazardAreas.erase(std::remove_if(hazardAreas.begin(), hazardAreas.end(), [](const HazardArea& h) { return !h.active; }), hazardAreas.end());
    effects.erase(std::remove_if(effects.begin(), effects.end(), [](const VisualEffect& e) { return e.life <= 0; }), effects.end());
    damageTexts.erase(std::remove_if(damageTexts.begin(), damageTexts.end(), [](const DamageText& t) { return t.life <= 0; }), damageTexts.end());
}

// ★ 毒沼エリア：プレイヤーへの溶解（腐蝕）状態異常付与 ＆ 耐性モディファイア判定
void EffectManager::UpdateHazards(float dt, Player& p) {
    for (auto& hz : hazardAreas) {
        if (!hz.active) continue;
        float dist = Vector3Distance(p.position, hz.pos);
        if (dist <= hz.radius + p.radius) {
            hz.tickTimer -= dt;
            if (hz.tickTimer <= 0.0f) {
                hz.tickTimer = 0.5f;

                float resist = p.GetPlayerElementResistance(hz.element);
                float finalDmg = fmaxf(2.0f, (hz.dps * 0.5f) * (1.0f - resist));

                p.hp -= finalDmg;
                SpawnDamageText(p.position, (int)finalDmg);
                SpawnEffect(Vector3Add(p.position, { 0, 0.3f, 0 }), { 0, 1, 0 }, FX_HIT, hz.color);
                TriggerDamageFlash(0.15f);

                std::string swampFmt = DataManager::uiStrings.count("LOG_POISON_SWAMP") ? DataManager::uiStrings["LOG_POISON_SWAMP"] : "POISON AREA HIT! -%d DMG";
                UI::AddSystemLog(TextFormat(swampFmt.c_str(), (int)finalDmg), hz.color);

                // 毒沼による状態異常（溶解・腐蝕）付与判定
                if (hz.element != ELEM_NONE && GetRandomValue(1, 100) <= 40) {
                    if (p.HasStatusImmunity(hz.element)) {
                        std::string immFmt = DataManager::uiStrings.count("LOG_STATUS_IMMUNE") ? DataManager::uiStrings["LOG_STATUS_IMMUNE"] : "★ STATUS IMMUNED! ★";
                        UI::AddSystemLog(immFmt, GREEN);
                    }
                    else {
                        p.status.ApplyStatus(hz.element, 3.5f, finalDmg);
                        std::string rotKey = "LOG_PLAYER_ROT";
                        if (DataManager::uiStrings.count(rotKey)) {
                            UI::AddSystemLog(DataManager::uiStrings[rotKey], Player::GetElementColor(hz.element));
                        }
                    }
                }
            }
        }
    }
}

Vector3 EffectManager::GetShakeOffset() const {
    if (shakeTimer <= 0.0f || shakeIntensity <= 0.0f) {
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
            for (auto& e : enemies) {
                float distXZ = Vector2Distance({ proj.pos.x, proj.pos.z }, { e.position.x, e.position.z });
                float distY = fabsf(proj.pos.y - (e.position.y + 0.4f));

                if (distXZ < (proj.radius + e.radius + 0.3f) && distY < 1.0f) {
                    bool isCrit = (GetRandomValue(1, 100) <= 15);

                    float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[p.activeSlot]);
                    int weaponElem = p.equippedData[p.activeSlot].element;
                    float elemMulti = Player::GetElementMultiplier(weaponElem, e.data.element);

                    // 杖からの攻撃力引き継ぎ対応
                    float baseAtk = (proj.damage > 0.0f) ? proj.damage : (p.attackPower + totalBonus);
                    int dmg = (int)(baseAtk * elemMulti) + GetRandomValue(0, 5);
                    if (isCrit) dmg = (int)((float)dmg * 1.75f);

                    e.hp -= (float)dmg;
                    e.hudTimer = 5.0f;
                    if (e.isBoss) e.delayedHpTimer = 0.5f;
                    e.ApplyKnockback(Vector3Normalize(proj.vel), isCrit ? 1.0f : 0.5f, d);

                    SpawnDamageText(e.position, dmg, isCrit);
                    SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, isCrit ? GOLD : PURPLE);

                    if (elemMulti > 1.1f) {
                        std::string weakFmt = DataManager::uiStrings.count("LOG_WEAKNESS_HIT") ? DataManager::uiStrings["LOG_WEAKNESS_HIT"] : "★ %s WEAKNESS! %d DMG! ★";
                        Color elemCol = Player::GetElementColor(weaponElem);
                        UI::AddSystemLog(TextFormat(weakFmt.c_str(), e.data.name.c_str(), dmg), elemCol);
                    }
                    else if (elemMulti < 0.9f) {
                        std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_ATTACK") ? DataManager::uiStrings["LOG_RESISTED_ATTACK"] : "[RESIST] %s: %d DMG";
                        UI::AddSystemLog(TextFormat(resFmt.c_str(), e.data.name.c_str(), dmg), LIGHTGRAY);
                    }
                    else if (isCrit) {
                        UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_CRIT_DEALT"].c_str(), e.data.name.c_str(), dmg), GOLD);
                        TriggerHitStop(0.06f);
                        ShakeScreen(0.15f, 0.3f);
                    }
                    else {
                        UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_DMG_DEALT"].c_str(), e.data.name.c_str(), dmg), ORANGE);
                    }

                    proj.active = false;
                    break;
                }
            }
        }
        else {
            float distXZ = Vector2Distance({ proj.pos.x, proj.pos.z }, { p.position.x, p.position.z });
            float distY = fabsf(proj.pos.y - (p.position.y + 0.6f));

            if (distXZ < (proj.radius + p.radius + 0.25f) && distY < 0.95f) {
                // ★ 敵の弾丸威力の適用（レベルスケール値）
                float rawDmg = (proj.damage > 0.0f) ? proj.damage : ((proj.type == 2) ? 18.0f : 15.0f);
                float defDmg = fmaxf(2.0f, rawDmg - p.defense);

                int projElem = proj.element;
                if (projElem == ELEM_NONE) {
                    projElem = (proj.type == 1) ? ELEM_ABYSS : ((proj.type == 2) ? ELEM_ROT : ELEM_NONE);
                }

                float resist = p.GetPlayerElementResistance(projElem);
                float finalDmg = fmaxf(2.0f, defDmg * (1.0f - resist));

                p.hp -= finalDmg;

                SpawnDamageText(p.position, (int)finalDmg);
                SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, (projElem == ELEM_ROT) ? GREEN : RED);
                ShakeScreen(0.15f, 0.3f);
                TriggerDamageFlash(0.25f);

                if (resist > 0.05f) {
                    std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_PROJ") ? DataManager::uiStrings["LOG_RESISTED_PROJ"] : "★ RESISTED! (-%d%%) ★";
                    UI::AddSystemLog(TextFormat(resFmt.c_str(), (int)(resist * 100)), SKYBLUE);
                }
                else {
                    std::string hitFmt = DataManager::uiStrings.count("LOG_ENEMY_HIT") ? DataManager::uiStrings["LOG_ENEMY_HIT"] : "ENEMY HIT: %d DMG!";
                    UI::AddSystemLog(TextFormat(hitFmt.c_str(), (int)finalDmg), RED);
                }

                // ★ 敵の属性弾によるプレイヤーへの状態異常付与 ＆ 耐性モディファイア判定
                if (projElem != ELEM_NONE && GetRandomValue(1, 100) <= 35) {
                    if (p.HasStatusImmunity(projElem)) {
                        std::string immFmt = DataManager::uiStrings.count("LOG_STATUS_IMMUNE") ? DataManager::uiStrings["LOG_STATUS_IMMUNE"] : "★ STATUS IMMUNED! ★";
                        UI::AddSystemLog(immFmt, GREEN);
                    }
                    else {
                        p.status.ApplyStatus(projElem, 3.5f, finalDmg);
                        std::string logKey = "";
                        if (projElem == ELEM_HELLFIRE) logKey = "LOG_PLAYER_BURN";
                        else if (projElem == ELEM_ROT) logKey = "LOG_PLAYER_ROT";
                        else if (projElem == ELEM_SOUL) logKey = "LOG_PLAYER_CHILL";
                        else if (projElem == ELEM_ABYSS) logKey = "LOG_PLAYER_CURSE";

                        if (!logKey.empty() && DataManager::uiStrings.count(logKey)) {
                            UI::AddSystemLog(DataManager::uiStrings[logKey], Player::GetElementColor(projElem));
                        }
                    }
                }

                proj.active = false;
            }
        }
    }
}

void EffectManager::Draw() {
    float time = (float)GetTime();
    for (const auto& hz : hazardAreas) {
        if (!hz.active) continue;
        float pulse = sinf(time * 4.0f) * 0.15f;
        float drawRadius = hz.radius + pulse;

        DrawCylinder(hz.pos, drawRadius, drawRadius, 0.03f, 24, Fade(hz.color, 0.32f));
        DrawCylinderWires(hz.pos, drawRadius, drawRadius, 0.04f, 24, Fade(hz.color, 0.75f));
        DrawCylinderWires(hz.pos, drawRadius * 0.6f, drawRadius * 0.6f, 0.04f, 16, Fade(WHITE, 0.45f));
    }

    for (const auto& p : projectiles) {
        Color c = (p.type == 0) ? YELLOW : ((p.type == 2) ? LIME : PURPLE);
        if (!p.isPlayer && p.type != 2) c = RED;

        DrawSphere(p.pos, p.radius * 0.7f, WHITE);
        DrawSphere(p.pos, p.radius, c);
        DrawSphere(p.pos, p.radius * 1.6f, Fade(c, 0.35f));

        Vector3 tail = Vector3Subtract(p.pos, Vector3Scale(Vector3Normalize(p.vel), 0.7f));
        DrawLine3D(p.pos, tail, Fade(WHITE, 0.8f));
    }

    for (const auto& e : effects) {
        float ratio = e.life / e.maxLife;
        Color c = Fade(e.color, ratio);

        if (e.type == FX_SLASH) {
            float baseAngle = atan2f(e.dir.z, e.dir.x);
            Vector3 center = e.pos;

            float progress = 1.0f - (e.life / e.maxLife);
            float pLead = progress * 1.5f;   if (pLead > 1.0f) pLead = 1.0f;
            float pTrail = (progress - 0.22f) * 1.5f; if (pTrail < 0.0f) pTrail = 0.0f;

            if (pLead > pTrail) {
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
                    float innerR = 0.6f;

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

                    DrawLine3D(pOut1, pOut2, Fade(WHITE, alpha2));
                }
            }
        }
        else if (e.type == FX_THRUST) {
            Vector3 start = e.pos;
            Vector3 end = Vector3Add(e.pos, Vector3Scale(e.dir, e.scale));

            DrawCylinderEx(start, end, 0.12f * ratio, 0.04f * ratio, 8, Fade(WHITE, ratio * 0.9f));
            DrawCylinderEx(start, end, 0.35f * ratio, 0.12f * ratio, 8, Fade(e.color, ratio * 0.45f));

            DrawSphere(end, 0.3f * ratio, WHITE);
            DrawSphere(end, 0.5f * ratio, Fade(e.color, ratio * 0.4f));
        }
        else if (e.type == FX_SMASH) {
            Vector3 impactPos = Vector3Add(e.pos, Vector3Scale(e.dir, e.scale * 0.6f));
            impactPos.y = 0.08f;

            float expand = (1.0f - ratio);
            float currentRadius = (e.scale * 0.4f) + expand * (e.scale * 0.6f);

            DrawCylinder(impactPos, currentRadius, currentRadius, 0.03f, 28, Fade(e.color, ratio * 0.45f));
            DrawCylinderWires(impactPos, currentRadius, currentRadius, 0.04f, 28, Fade(GOLD, ratio * 0.9f));
            DrawCylinderWires(impactPos, currentRadius * 0.7f, currentRadius * 0.7f, 0.04f, 28, Fade(WHITE, ratio * 0.6f));
        }
        else if (e.type == FX_HIT) {
            DrawSphere(e.pos, 0.2f * ratio, WHITE);
            DrawSphere(e.pos, 0.4f * ratio, Fade(e.color, ratio * 0.5f));
        }
        else if (e.type == FX_EXPLOSION) {
            float expand = (1.0f - ratio);
            float rExplode = e.scale * (0.3f + expand * 1.5f);
            Vector3 pos = e.pos;

            DrawSphere(pos, rExplode * 0.5f, Fade(WHITE, ratio * 0.9f));
            DrawSphere(pos, rExplode * 0.8f, Fade(e.color, ratio * 0.6f));
            DrawSphere(pos, rExplode, Fade(GOLD, ratio * 0.3f));

            Vector3 ringPos = { pos.x, 0.15f, pos.z };
            float ringR = e.scale * (0.5f + expand * 2.2f);
            DrawCylinderWires(ringPos, ringR, ringR, 0.05f, 32, Fade(WHITE, ratio * 0.9f));
            DrawCylinderWires(ringPos, ringR * 0.8f, ringR * 0.8f, 0.04f, 32, Fade(e.color, ratio * 0.7f));
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