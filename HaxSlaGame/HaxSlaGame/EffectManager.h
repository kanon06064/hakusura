#pragma once
#include "Definitions.h"
#include <vector>

class Dungeon;
class Enemy;
class Player;

class EffectManager {
public:
    std::vector<Projectile> projectiles;   // 飛んでいく魔法や矢
    std::vector<VisualEffect> effects;     // 斬撃などの一瞬だけ残るエフェクト
    std::vector<DamageText> damageTexts;   // 「15」などのダメージ表記テキスト

    void Update(float dt, Dungeon& d);
    void Draw(); // 3D空間にエフェクトを描画
    void Draw2D(Font font, Camera3D cam); // スクリーン空間(2D)にテキストを描画

    // エフェクトを発生させる関数群
    void SpawnProjectile(Vector3 pos, Vector3 dir, float speed, int type, bool isPlayer);
    void SpawnEffect(Vector3 pos, Vector3 dir, EffectType type, Color col);
    void SpawnDamageText(Vector3 pos, int dmg);

    // プロジェクタイルの当たり判定を処理する
    void CheckProjectileCollisions(std::vector<Enemy>& enemies, Player& p, Dungeon& d);
};