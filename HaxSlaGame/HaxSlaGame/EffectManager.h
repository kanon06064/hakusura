#pragma once
#include "Definitions.h"
#include <vector>

class Dungeon;
class Enemy;
class Player;

class EffectManager {
public:
    std::vector<Projectile> projectiles;
    std::vector<VisualEffect> effects;
    std::vector<DamageText> damageTexts;
    std::vector<HazardArea> hazardAreas;

    float shakeTimer = 0.0f;
    float shakeIntensity = 0.0f;

    void ShakeScreen(float duration, float intensity) {
        if (intensity >= shakeIntensity || shakeTimer <= 0.0f) {
            shakeTimer = duration;
            shakeIntensity = intensity;
        }
    }

    Vector3 GetShakeOffset() const;

    float hitStopTimer = 0.0f;
    void TriggerHitStop(float duration) {
        if (duration > hitStopTimer) {
            hitStopTimer = duration;
        }
    }

    float damageFlashTimer = 0.0f;
    void TriggerDamageFlash(float duration = 0.25f) {
        damageFlashTimer = duration;
    }

    float slowMotionTimer = 0.0f;
    float slowMotionFactor = 1.0f;
    float victoryBannerTimer = 0.0f;

    void TriggerBossDefeated();

    void SpawnHazardArea(Vector3 pos, float radius, float duration, float dps, Color col, int element = ELEM_ROT);

    void Update(float dt, Dungeon& d);
    void UpdateHazards(float dt, Player& p);
    void Draw();
    void Draw2D(Font font, Camera3D cam);

    void SpawnProjectile(Vector3 pos, Vector3 dir, float speed, int type, bool isPlayer, float damage = 15.0f, int element = ELEM_NONE);
    void SpawnEffect(Vector3 pos, Vector3 dir, EffectType type, Color col, float scale = 1.0f);
    void SpawnDamageText(Vector3 pos, int dmg, bool isCrit = false);

    void CheckProjectileCollisions(std::vector<Enemy>& enemies, Player& p, Dungeon& d);
};