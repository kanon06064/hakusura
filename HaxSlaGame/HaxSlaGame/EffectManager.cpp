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

void EffectManager::SpawnEffect(Vector3 pos, Vector3 dir, EffectType type, Color col) {
    VisualEffect eff;
    eff.pos = pos;
    eff.dir = dir;
    eff.type = type;
    eff.color = col;
    eff.life = 0.3f;    // 0.3秒で消滅する
    eff.maxLife = 0.3f;
    effects.push_back(eff);
}

void EffectManager::SpawnDamageText(Vector3 pos, int dmg) {
    damageTexts.push_back({ Vector3Add(pos, {0, 1.5f, 0}), dmg, 1.0f }); // 1秒間表示
}

void EffectManager::Update(float dt, Dungeon& d) {
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
                    // ダメージ計算: 基本ATK + 武器のATK + 乱数(0~5)
                    float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[p.activeSlot]);
                    int dmg = (int)(p.attackPower + totalBonus) + GetRandomValue(0, 5);
                    e.hp -= (float)dmg;
                    e.hudTimer = 5.0f; // 敵のHPバーを表示させる
                    e.ApplyKnockback(Vector3Normalize(proj.vel), 0.5f, d); // 弾の飛んだ方向にノックバック

                    SpawnDamageText(e.position, dmg);
                    SpawnEffect(proj.pos, { 0,0,0 }, FX_HIT, GOLD);

                    proj.active = false; // 当たったので弾を消す
                    break; // 貫通しない
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

                proj.active = false;
            }
        }
    }
}

void EffectManager::Draw() {
    for (const auto& p : projectiles) {
        Color c = (p.type == 0) ? YELLOW : PURPLE;
        if (!p.isPlayer) c = RED;
        DrawSphere(p.pos, p.radius, c); // 弾本体の描画
        if (p.type == 0) DrawLine3D(p.pos, Vector3Subtract(p.pos, Vector3Scale(Vector3Normalize(p.vel), 0.5f)), WHITE); // 軌跡
    }

    for (const auto& e : effects) {
        float ratio = e.life / e.maxLife; // 寿命に応じて徐々に透明にする
        Color c = Fade(e.color, ratio);

        if (e.type == FX_SLASH) { // 剣の斬撃エフェクト(扇状の線)
            float baseAngle = atan2f(e.dir.z, e.dir.x);
            Vector3 center = e.pos;
            for (int i = -60; i <= 60; i += 10) {
                float rad = baseAngle + (i * DEG2RAD);
                Vector3 tip = { center.x + cosf(rad) * 4.0f, center.y, center.z + sinf(rad) * 4.0f };
                DrawLine3D(center, tip, c);
            }
        }
        else if (e.type == FX_THRUST) { // 槍の突きエフェクト(直線)
            Vector3 start = e.pos;
            Vector3 end = Vector3Add(e.pos, Vector3Scale(e.dir, 5.0f));
            DrawLine3D(start, end, c);
            DrawSphere(end, 0.4f * ratio, c);
        }
        else if (e.type == FX_SMASH) { // 斧の強撃エフェクト(円形)
            DrawSphereWires(Vector3Add(e.pos, Vector3Scale(e.dir, 3.0f)), 3.0f, 8, 8, c);
        }
        else if (e.type == FX_HIT) {   // ヒット時の火花エフェクト
            DrawSphereWires(e.pos, 0.5f + (1.0f - ratio), 6, 6, c);
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
        else {
            Color c = ORANGE;
            if (dt.amount > 20) c = RED; // 大ダメージは赤色にする
            DrawTextEx(font, TextFormat("%d", dt.amount), { s.x, s.y }, 24, 1, Fade(c, dt.life));
        }
    }
}