#include "StatusEffect.h"
#include "EffectManager.h"
#include "UI.h"
#include "raymath.h"
#include <algorithm>

void StatusManager::ApplyStatus(int element, float duration, float power) {
    switch (element) {
    case 1: // ELEM_HELLFIRE -> ‰Šã
        burn.type = STATUS_BURN;
        burn.timer = duration;
        burn.maxTimer = duration;
        burn.power = power;
        burn.tickTimer = 0.5f; // 0.5•b‚²‚Æ‚ÉDot
        break;
    case 2: // ELEM_ROT -> —n‰ð
        rot.type = STATUS_ROT;
        rot.timer = duration;
        rot.maxTimer = duration;
        rot.power = 0.30f; // –hŒä—Í -30%
        break;
    case 3: // ELEM_SOUL -> “€
        chill.type = STATUS_CHILL;
        chill.timer = duration;
        chill.maxTimer = duration;
        chill.power = 0.50f; // ‘¬“x -50%
        break;
    case 4: // ELEM_ABYSS -> ŠŽã
        curse.type = STATUS_CURSE;
        curse.timer = duration;
        curse.maxTimer = duration;
        curse.power = 0.25f; // “GUŒ‚—Í -25%
        break;
    }
}

void StatusManager::Update(float dt, float& currentHp, EffectManager& fx, Vector3 pos) {
    // 1. ‰ŠãiDotƒ_ƒ[ƒWj
    if (burn.timer > 0.0f) {
        burn.timer -= dt;
        burn.tickTimer -= dt;
        if (burn.tickTimer <= 0.0f) {
            burn.tickTimer = 0.5f; // 0.5•b‚²‚Æ‚É¬ƒ_ƒ[ƒW
            int dotDmg = (int)fmaxf(3.0f, burn.power * 0.15f);
            currentHp -= (float)dotDmg;
            fx.SpawnDamageText(pos, dotDmg);
            fx.SpawnEffect(Vector3Add(pos, { 0, 0.5f, 0 }), { 0, 1, 0 }, FX_HIT, RED);
        }
        if (burn.timer <= 0.0f) burn.type = STATUS_NONE;
    }

    // 2. —n‰ð
    if (rot.timer > 0.0f) {
        rot.timer -= dt;
        if (rot.timer <= 0.0f) rot.type = STATUS_NONE;
    }

    // 3. “€
    if (chill.timer > 0.0f) {
        chill.timer -= dt;
        if (chill.timer <= 0.0f) chill.type = STATUS_NONE;
    }

    // 4. ŠŽã
    if (curse.timer > 0.0f) {
        curse.timer -= dt;
        if (curse.timer <= 0.0f) curse.type = STATUS_NONE;
    }
}

float StatusManager::GetSpeedMultiplier() const {
    return (chill.timer > 0.0f) ? 0.50f : 1.0f; // “€‚ÅˆÚ“®‘¬“x”¼Œ¸
}

float StatusManager::GetDefenseReductionRate() const {
    return (rot.timer > 0.0f) ? rot.power : 0.0f; // —n‰ð‚Å–hŒä—Í30%Œ¸
}

float StatusManager::GetAttackReductionRate() const {
    return (curse.timer > 0.0f) ? curse.power : 0.0f; // ŠŽã‚Å“GUŒ‚—Í25%Œ¸
}

bool StatusManager::HasAnyStatus() const {
    return (burn.timer > 0 || rot.timer > 0 || chill.timer > 0 || curse.timer > 0);
}

void StatusManager::DrawAura(Vector3 pos, float radius) {
    float time = (float)GetTime();
    Vector3 auraPos = { pos.x, 0.08f, pos.z };

    // ‰ŠãF‘«Œ³‚É‰ñ“]‚·‚é[g‚ÌƒŠƒ“ƒO
    if (burn.timer > 0.0f) {
        float r = radius * 1.2f + sinf(time * 6.0f) * 0.1f;
        DrawCylinderWires(auraPos, r, r, 0.03f, 16, Fade(RED, 0.7f));
    }
    // —n‰ðF“Å—Î‚Ì–A—§‚ÂƒŠƒ“ƒO
    if (rot.timer > 0.0f) {
        float r = radius * 1.3f + cosf(time * 5.0f) * 0.15f;
        DrawCylinderWires(auraPos, r, r, 0.03f, 16, Fade(GREEN, 0.7f));
    }
    // “€F—â‹CE•X‚Ì‘“‚¢ƒŠƒ“ƒO
    if (chill.timer > 0.0f) {
        float r = radius * 1.1f;
        DrawCylinder(auraPos, r, r, 0.02f, 16, Fade(SKYBLUE, 0.35f));
        DrawCylinderWires(auraPos, r, r, 0.03f, 16, Fade(WHITE, 0.8f));
    }
    // ŠŽãF[•£‚ÌŽ‡‚ÌèÉƒŠƒ“ƒO
    if (curse.timer > 0.0f) {
        float r = radius * 1.4f + sinf(time * 3.0f) * 0.2f;
        DrawCylinder(auraPos, r, r, 0.02f, 16, Fade(PURPLE, 0.3f));
        DrawCylinderWires(auraPos, r, r, 0.03f, 16, Fade(MAGENTA, 0.7f));
    }
}