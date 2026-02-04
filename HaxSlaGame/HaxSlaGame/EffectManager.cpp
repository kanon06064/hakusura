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
    // 弾が地面に埋まらないように少し高さを確保
    if (p.pos.y < 0.5f) p.pos.y = 1.2f;
    p.vel = Vector3Scale(Vector3Normalize(dir), speed);
    p.radius = (type == 1) ? 0.6f : 0.2f; // 魔法なら大きく、矢なら小さく
    p.active = true;
    p.type = type;
    p.isPlayer = isPlayer;
    projectiles.push_back(p);
}

void EffectManager::SpawnEffect(Vector3 pos, Vector3 dir, EffectType type, Color col) {
    VisualEffect eff;
    eff.pos = pos;
    eff.dir = dir;
    eff.type = type;
    eff.color = col;
    eff.life = 0.3f; // 表示時間
    eff.maxLife = 0.3f;
    effects.push_back(eff);
}

void EffectManager::SpawnDamageText(Vector3 pos, int dmg) {
    damageTexts.push_back({ Vector3Add(pos, {0, 1.5f, 0}), dmg, 1.0f });
}

void EffectManager::Update(float dt, Dungeon& d) {
    // 弾の更新
    for (auto& p : projectiles) {
        if (!p.active) continue;
        p.pos = Vector3Add(p.pos, Vector3Scale(p.vel, dt));
        // 壁衝突
        if (d.IsWall(p.pos.x, p.pos.z)) p.active = false;
        // マップ外へ出たら消す
        if (p.pos.x < 0 || p.pos.x > MAP_WIDTH * TILE_SIZE || p.pos.z < 0 || p.pos.z > MAP_HEIGHT * TILE_SIZE) p.active = false;
    }

    // エフェクト更新
    for (auto& e : effects) e.life -= dt;

    // ダメージテキスト更新
    for (auto& t : damageTexts) {
        t.life -= dt;
        t.pos.y += 0.5f * dt; // 少し上昇
    }

    // クリーンアップ
    projectiles.erase(std::remove_if(projectiles.begin(), projectiles.end(), [](const Projectile& p) { return !p.active; }), projectiles.end());
    effects.erase(std::remove_if(effects.begin(), effects.end(), [](const VisualEffect& e) { return e.life <= 0; }), effects.end());
    damageTexts.erase(std::remove_if(damageTexts.begin(), damageTexts.end(), [](const DamageText& t) { return t.life <= 0; }), damageTexts.end());
}

void EffectManager::CheckProjectileCollisions(std::vector<Enemy>& enemies, Player& p, Dungeon& d) {
    for (auto& proj : projectiles) {
        if (!proj.active) continue;

        if (proj.isPlayer) {
            // プレイヤーの弾 -> 敵への判定
            for (auto& e : enemies) {
                // 当たり判定: 弾の半径 + 敵の半径 + マージン
                if (Vector3Distance(proj.pos, e.position) < (proj.radius + e.radius + 0.3f)) {
                    int dmg = (int)(p.attackPower + p.equippedData[p.activeSlot].atkBonus) + GetRandomValue(0, 5);
                    e.hp -= (float)dmg;
                    e.hudTimer = 5.0f;
                    e.ApplyKnockback(Vector3Normalize(proj.vel), 0.5f, d);

                    SpawnDamageText(e.position, dmg);
                    SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, GOLD); // ヒットエフェクト

                    proj.active = false;
                    break;
                }
            }
        }
        else {
            // 敵の弾 -> プレイヤーへの判定
            if (Vector3Distance(proj.pos, p.position) < (proj.radius + p.radius + 0.2f)) {
                float rawDmg = 12.0f; // 固定ダメージまたは敵データ参照
                float dmg = fmaxf(1.0f, rawDmg - p.defense);
                p.hp -= dmg;

                SpawnDamageText(p.position, (int)dmg);
                SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, RED);

                proj.active = false;
            }
        }
    }
}

void EffectManager::Draw() {
    // 弾の描画
    for (const auto& p : projectiles) {
        Color c = (p.type == 0) ? YELLOW : PURPLE; // 0:矢, 1:魔法
        if (!p.isPlayer) c = RED; // 敵の弾は赤
        DrawSphere(p.pos, p.radius, c);
        // 軌跡
        if (p.type == 0) DrawLine3D(p.pos, Vector3Subtract(p.pos, Vector3Scale(Vector3Normalize(p.vel), 0.5f)), WHITE);
    }

    // エフェクトの描画
    for (const auto& e : effects) {
        float ratio = e.life / e.maxLife;
        Color c = Fade(e.color, ratio);

        if (e.type == FX_SLASH) {
            // 剣の扇状エフェクト
            float baseAngle = atan2f(e.dir.z, e.dir.x);
            Vector3 center = e.pos;
            for (int i = -60; i <= 60; i += 10) {
                float rad = baseAngle + (i * DEG2RAD);
                Vector3 tip = { center.x + cosf(rad) * 4.0f, center.y, center.z + sinf(rad) * 4.0f };
                DrawLine3D(center, tip, c);
            }
        }
        else if (e.type == FX_THRUST) {
            // 槍の突きエフェクト
            Vector3 start = e.pos;
            Vector3 end = Vector3Add(e.pos, Vector3Scale(e.dir, 5.0f));
            DrawLine3D(start, end, c);
            DrawSphere(end, 0.4f * ratio, c);
        }
        else if (e.type == FX_SMASH) {
            // 斧用
            DrawSphereWires(Vector3Add(e.pos, Vector3Scale(e.dir, 3.0f)), 3.0f, 8, 8, c);
        }
        else if (e.type == FX_HIT) {
            // ヒット時の粒子
            DrawSphereWires(e.pos, 0.5f + (1.0f - ratio), 6, 6, c);
        }
    }
}

void EffectManager::Draw2D(Font font, Camera3D cam) {
    for (const auto& dt : damageTexts) {
        Vector2 s = GetWorldToScreen(dt.pos, cam);
        // 画面外チェック（簡易）
        if (s.x < 0 || s.y < 0 || s.x > GetScreenWidth() || s.y > GetScreenHeight()) continue;

        if (dt.amount == 999) {
            DrawTextEx(font, "LEVEL UP!!", { s.x - 50, s.y - 30 }, 28, 1, YELLOW);
        }
        else {
            Color c = ORANGE;
            if (dt.amount > 20) c = RED; // 大ダメージ
            DrawTextEx(font, TextFormat("%d", dt.amount), { s.x, s.y }, 24, 1, Fade(c, dt.life));
        }
    }
}