#if defined(_MSC_VER) && (_MSC_VER >= 1900)
#pragma execution_character_set("utf-8")
#endif

#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "raymath.h"
#include <math.h>

Player::Player(Vector3 sp) : position(sp), speed(0.15f), radius(0.45f), attackTimer(0), visualTimer(0), isAttacking(false), lastAimDir({ 1,0,0 }), hp(100), maxHp(100), attackPower(12), defense(5), level(1), exp(0), expToNext(100), skillPoints(0) {
    equippedWeapons[0] = SWORD; equippedWeapons[1] = BOW; activeSlot = 0; currentWeapon = equippedWeapons[0];
}

void Player::AddExp(int a, std::vector<DamageText>& dt) { exp += a; if (exp >= expToNext) LevelUp(dt); }
void Player::LevelUp(std::vector<DamageText>& dt) {
    level++; exp -= expToNext; expToNext = (int)((float)expToNext * 1.5f);
    maxHp += 20; hp = maxHp; attackPower += 2.5f; defense += 1.5f; skillPoints += 3;
    dt.push_back({ {position.x, position.y + 2, position.z}, 999, 2 });
}
void Player::AddToInventory(ItemData item) { for (auto& i : inventory) { if (i.id == item.id) { i.count++; return; } } inventory.push_back(item); }
void Player::UseItem(int idx) {
    if (idx < 0 || idx >= (int)inventory.size()) return;
    if (inventory[idx].type == "CONSUMABLE") { hp = fminf(maxHp, hp + inventory[idx].heal); inventory[idx].count--; if (inventory[idx].count <= 0) inventory.erase(inventory.begin() + idx); }
}
void Player::EquipWeapon(int idx, int slot) {
    if (idx < 0 || idx >= (int)inventory.size()) return;
    if (inventory[idx].type == "EQUIP") { equippedWeapons[slot] = (WeaponType)inventory[idx].weaponSubtype; if (activeSlot == slot) currentWeapon = equippedWeapons[slot]; }
}
void Player::UpgradeStat(int t) { if (skillPoints <= 0) return; if (t == 0) attackPower += 5; else if (t == 1) defense += 3; else if (t == 2) { maxHp += 50; hp = maxHp; } skillPoints--; }

void Player::Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, std::vector<DamageText>& dt, bool stop) {
    if (stop) return;
    if (IsKeyPressed(KEY_Q)) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }

    // --- カメラの現在の角度から移動ベクトルを算出（カメラ基準移動） ---
    Vector3 camForward = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
    camForward.y = 0; camForward = Vector3Normalize(camForward);
    Vector3 camRight = { -camForward.z, 0, camForward.x };

    Vector3 md = { 0, 0, 0 };
    if (IsKeyDown(KEY_W)) md = Vector3Add(md, camForward);
    if (IsKeyDown(KEY_S)) md = Vector3Subtract(md, camForward);
    if (IsKeyDown(KEY_A)) md = Vector3Subtract(md, camRight);
    if (IsKeyDown(KEY_D)) md = Vector3Add(md, camRight);

    if (Vector3Length(md) > 0) {
        md = Vector3Normalize(md);
        Vector3 v = Vector3Scale(md, speed);
        if (!d.CheckCollisionRadius(Vector3Add(position, { v.x, 0, 0 }), radius)) position.x += v.x;
        if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, v.z }), radius)) position.z += v.z;
    }

    Ray ray = GetMouseRay(GetMousePosition(), cam);
    float t = -ray.position.y / ray.direction.y;
    Vector3 g = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    lastAimDir = Vector3Normalize(Vector3Subtract(g, position)); lastAimDir.y = 0;

    if (attackTimer > 0) attackTimer -= GetFrameTime();
    if (visualTimer > 0) visualTimer -= GetFrameTime(); else isAttacking = false;
    if (IsMouseButtonPressed(0) && attackTimer <= 0) {
        PerformAttack(lastAimDir, enemies, d, dt);
        attackTimer = (currentWeapon >= BOW) ? 0.35f : 0.5f; isAttacking = true; visualTimer = 0.15f;
    }

    // 弾丸更新
    for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
        projectiles[i].pos = Vector3Add(projectiles[i].pos, Vector3Scale(projectiles[i].vel, GetFrameTime()));
        if (d.IsWall(projectiles[i].pos.x, projectiles[i].pos.z)) { projectiles.erase(projectiles.begin() + i); continue; }
        for (auto& e : enemies) {
            if (Vector3Distance(projectiles[i].pos, e.position) < (projectiles[i].radius + e.radius)) {
                int dmg = (int)attackPower + GetRandomValue(0, 5); e.hp -= (float)dmg; e.hudTimer = 5;
                e.ApplyKnockback(Vector3Normalize(projectiles[i].vel), 0.5f, d);
                dt.push_back({ {e.position.x, 1.5f, e.position.z}, dmg, 0.8f });
                projectiles.erase(projectiles.begin() + i); break;
            }
        }
    }
}

void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, std::vector<DamageText>& dt) {
    if (currentWeapon == BOW) projectiles.push_back({ position, Vector3Scale(ad, 25), 0.2f, true, 0 });
    else if (currentWeapon == WAND) projectiles.push_back({ position, Vector3Scale(ad, 15), 0.5f, true, 1 });
    else {
        int hc = 0;
        for (auto& e : enemies) {
            if (!d.HasLineOfSight(position, e.position)) continue;
            Vector3 v = Vector3Subtract(e.position, position); float dist = Vector3Length(v);
            bool hit = false; float knk = 0; float mult = 1.0f;
            switch (currentWeapon) {
            case SWORD: if (dist < 4.5f && Vector3DotProduct(ad, Vector3Normalize(v)) > cosf(60 * DEG2RAD)) { hit = true; knk = 0.8f; mult = 1.0f; } break;
            case SPEAR: { float f = Vector3DotProduct(v, ad), s = fabsf(Vector3DotProduct(v, { -ad.z,0,ad.x })); if (f > 0 && f < 8.2f && s < 1.3f) { hit = true; knk = 2.2f; } } break;
            case AXE: if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(ad, 3.5f))) < 3.5f) { hit = true; knk = 1.3f; mult = 1.5f; } break;
            }
            if (hit) {
                int dmg = (int)(attackPower * mult) + GetRandomValue(-2, 3); e.hp -= (float)dmg; e.hudTimer = 5; e.ApplyKnockback(ad, knk, d);
                dt.push_back({ {e.position.x, 1.5f - ((float)hc * 0.4f), e.position.z}, dmg, 0.8f }); hc++;
            }
        }
    }
}

void Player::Draw() {
    DrawCube(position, 1, 1, 1, RED); DrawCubeWires(position, 1, 1, 1, MAROON);
    for (auto& p : projectiles) { if (p.type == 0) DrawCube(p.pos, 0.2f, 0.2f, 0.6f, BROWN); else DrawSphere(p.pos, p.radius, SKYBLUE); }
    if (isAttacking) {
        float b = atan2f(lastAimDir.z, lastAimDir.x);
        if (currentWeapon == SWORD) { for (int i = -60; i <= 60; i += 10) DrawLine3D({ position.x, 0.15f, position.z }, { position.x + cosf(b + (float)i * DEG2RAD) * 4.5f, 0.15f, position.z + sinf(b + (float)i * DEG2RAD) * 4.5f }, YELLOW); }
        else if (currentWeapon == SPEAR) { Vector3 r = { -lastAimDir.z,0,lastAimDir.x }, p1 = Vector3Add(position, Vector3Scale(r, 1.3f)), p2 = Vector3Subtract(position, Vector3Scale(r, 1.3f)), p3 = Vector3Add(p2, Vector3Scale(lastAimDir, 8.2f)), p4 = Vector3Add(p1, Vector3Scale(lastAimDir, 8.2f)); p1.y = p2.y = p3.y = p4.y = 0.15f; DrawLine3D(p1, p2, YELLOW); DrawLine3D(p2, p3, YELLOW); DrawLine3D(p3, p4, YELLOW); DrawLine3D(p4, p1, YELLOW); }
        else if (currentWeapon == AXE) DrawSphereWires(Vector3Add(position, Vector3Scale(lastAimDir, 3.5f)), 3.5f, 8, 8, YELLOW);
    }
}