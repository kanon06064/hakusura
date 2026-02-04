#pragma once
#include "Definitions.h"
#include <vector>

// 前方宣言
class Dungeon;
class Enemy;
class Player;

class EffectManager {
public:
    std::vector<Projectile> projectiles;
    std::vector<VisualEffect> effects;
    std::vector<DamageText> damageTexts;

    void Update(float dt, Dungeon& d);
    void Draw();
    void Draw2D(Font font, Camera3D cam); // ダメージテキスト用

    // 生成メソッド
    void SpawnProjectile(Vector3 pos, Vector3 dir, float speed, int type, bool isPlayer);
    void SpawnEffect(Vector3 pos, Vector3 dir, EffectType type, Color col);
    void SpawnDamageText(Vector3 pos, int dmg);

    // 衝突判定
    void CheckProjectileCollisions(std::vector<Enemy>& enemies, Player& p, Dungeon& d);
};