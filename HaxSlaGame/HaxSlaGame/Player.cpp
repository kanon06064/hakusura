#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "raymath.h"
#include <math.h>

Player::Player(Vector3 sp) : position(sp), speed(0.15f), radius(0.45f), attackTimer(0), visualTimer(0), isAttacking(false), lastAimDir(Vector3{ 1,0,0 }), hp(100), maxHp(100), attackPower(12), defense(5), level(1), exp(0), expToNext(100), skillPoints(0) {
    equippedWeapons[0] = SWORD; equippedWeapons[1] = BOW; activeSlot = 0; currentWeapon = equippedWeapons[0];
    equippedData[0] = ItemData{ 0, "ƒ{ƒ‚ÌŒ•", "EQUIP", 0, 10, 0, 0, 1 };
    equippedData[1] = ItemData{ 0, "–Ø‚Ì‹|", "EQUIP", 0, 5, 0, 3, 1 };
    InitSkillTree();
}
void Player::InitSkillTree() {
    skillTree.push_back({ 0, "START", Vector2{400, 300}, {}, true, 0 });
    skillTree.push_back({ 1, "ATK UP I", Vector2{550, 200}, {0}, false, 1, 5, 0, 0 });
    skillTree.push_back({ 2, "DEF UP I", Vector2{550, 400}, {0}, false, 1, 0, 3, 0 });
    skillTree.push_back({ 3, "HP UP I", Vector2{350, 450}, {0}, false, 1, 0, 0, 50 });
    skillTree.push_back({ 4, "ATK UP II", Vector2{700, 200}, {1}, false, 2, 10, 0, 0 });
    skillTree.push_back({ 5, "MASTER", Vector2{850, 300}, {4, 2}, false, 5, 20, 10, 100 });
}
bool Player::IsSkillAvailable(int id) {
    if (skillTree[id].unlocked) return false;
    if (skillTree[id].reqIds.empty()) return true;
    for (int req : skillTree[id].reqIds) if (skillTree[req].unlocked) return true;
    return false;
}
void Player::UnlockSkill(int id) {
    if (IsSkillAvailable(id) && skillPoints >= skillTree[id].cost) {
        skillPoints -= skillTree[id].cost; skillTree[id].unlocked = true;
        attackPower += skillTree[id].atkAdd; defense += skillTree[id].defAdd; maxHp += skillTree[id].hpAdd; hp += skillTree[id].hpAdd;
    }
}
void Player::AddExp(int a, std::vector<DamageText>& dt) { exp += a; if (exp >= expToNext) LevelUp(dt); }
void Player::LevelUp(std::vector<DamageText>& dt) {
    level++; exp -= expToNext; expToNext = (int)((float)expToNext * 1.5f);
    maxHp += 20; hp = maxHp; attackPower += 2; defense += 1.5f; skillPoints += 3;
    dt.push_back(DamageText{ Vector3Add(position, Vector3{0, 2, 0}), 999, 2.0f });
}
bool Player::AddToInventory(ItemData item) {
    if (item.type == "EQUIP") { if ((int)inventoryEquip.size() >= MAX_EQUIP_INV) return false; inventoryEquip.push_back(item); }
    else { for (auto& i : inventoryItems) { if (i.id == item.id && i.count < MAX_ITEM_STACK) { i.count++; return true; } } if ((int)inventoryItems.size() >= MAX_ITEM_TYPES) return false; inventoryItems.push_back(item); }
    return true;
}
void Player::UseItem(int idx) {
    if (idx < 0 || idx >= (int)inventoryItems.size()) return;
    if (inventoryItems[idx].type == "CONSUMABLE") { hp = fminf(maxHp, hp + inventoryItems[idx].heal); inventoryItems[idx].count--; if (inventoryItems[idx].count <= 0) inventoryItems.erase(inventoryItems.begin() + idx); }
}
void Player::EquipWeapon(int idx, int slot) {
    if (idx < 0 || idx >= (int)inventoryEquip.size()) return;
    equippedData[slot] = inventoryEquip[idx]; equippedWeapons[slot] = (WeaponType)equippedData[slot].weaponSubtype;
    if (activeSlot == slot) currentWeapon = equippedWeapons[slot];
}
void Player::UpgradeStat(int t) { if (skillPoints <= 0) return; if (t == 0) attackPower += 5; else if (t == 1) defense += 3; else if (t == 2) { maxHp += 50; hp = maxHp; } skillPoints--; }

void Player::Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, std::vector<DamageText>& dt, bool stop) {
    if (stop) return;
    if (IsKeyPressed(KEY_Q)) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }
    Vector3 cf = Vector3Normalize(Vector3Subtract(cam.target, cam.position)); cf.y = 0; cf = Vector3Normalize(cf);
    Vector3 cr = Vector3{ -cf.z, 0, cf.x };
    Vector3 md = Vector3{ 0, 0, 0 };
    if (IsKeyDown(KEY_W)) md = Vector3Add(md, cf); if (IsKeyDown(KEY_S)) md = Vector3Subtract(md, cf);
    if (IsKeyDown(KEY_A)) md = Vector3Subtract(md, cr); if (IsKeyDown(KEY_D)) md = Vector3Add(md, cr);
    if (Vector3Length(md) > 0) {
        md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, speed);
        if (!d.CheckCollisionRadius(Vector3Add(position, Vector3{ v.x, 0, 0 }), radius)) position.x += v.x;
        if (!d.CheckCollisionRadius(Vector3Add(position, Vector3{ 0, 0, v.z }), radius)) position.z += v.z;
    }
    Ray ray = GetMouseRay(GetMousePosition(), cam); float t = -ray.position.y / ray.direction.y;
    Vector3 g = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
    lastAimDir = Vector3Normalize(Vector3Subtract(g, position)); lastAimDir.y = 0;
    if (attackTimer > 0) attackTimer -= GetFrameTime();
    if (visualTimer > 0) visualTimer -= GetFrameTime(); else isAttacking = false;
    if (IsMouseButtonPressed(0) && attackTimer <= 0) { PerformAttack(lastAimDir, enemies, d, dt); attackTimer = (currentWeapon >= BOW) ? 0.35f : 0.5f; isAttacking = true; visualTimer = 0.15f; }
    for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
        projectiles[i].active = true;
        projectiles[i].pos = Vector3Add(projectiles[i].pos, Vector3Scale(projectiles[i].vel, GetFrameTime()));
        if (d.IsWall(projectiles[i].pos.x, projectiles[i].pos.z)) { projectiles.erase(projectiles.begin() + i); continue; }
        for (auto& e : enemies) {
            if (Vector3Distance(projectiles[i].pos, e.position) < (projectiles[i].radius + e.radius)) {
                int dmg = (int)attackPower + (int)equippedData[activeSlot].atkBonus + GetRandomValue(0, 5); e.hp -= (float)dmg; e.hudTimer = 5;
                e.ApplyKnockback(Vector3Normalize(projectiles[i].vel), 0.5f, d);
                dt.push_back(DamageText{ Vector3{e.position.x, 1.5f, e.position.z}, dmg, 0.8f }); projectiles.erase(projectiles.begin() + i); break;
            }
        }
    }
}
void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, std::vector<DamageText>& dt) {
    if (currentWeapon == BOW) projectiles.push_back(Projectile{ position, Vector3Scale(ad, 25), 0.2f, true, 0 });
    else if (currentWeapon == WAND) projectiles.push_back(Projectile{ position, Vector3Scale(ad, 15), 0.5f, true, 1 });
    else {
        int hc = 0;
        for (auto& e : enemies) {
            if (!d.HasLineOfSight(position, e.position)) continue;
            Vector3 v = Vector3Subtract(e.position, position); float dist = Vector3Length(v);
            bool hit = false; float knk = 0; float mult = 1.0f;
            switch (currentWeapon) {
            case SWORD: if (dist < 4.5f && Vector3DotProduct(ad, Vector3Normalize(v)) > cosf(60 * DEG2RAD)) { hit = true; knk = 0.8f; mult = 1.0f; } break;
            case SPEAR: { float f = Vector3DotProduct(v, ad), s = fabsf(Vector3DotProduct(v, Vector3{ -ad.z,0,ad.x })); if (f > 0 && f < 8.2f && s < 1.3f) { hit = true; knk = 2.2f; } } break;
            case AXE: if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(ad, 3.5f))) < 3.5f) { hit = true; knk = 1.3f; mult = 1.5f; } break;
            }
            if (hit) {
                int dmg = (int)((attackPower + equippedData[activeSlot].atkBonus) * mult) + GetRandomValue(-2, 3); e.hp -= (float)dmg; e.hudTimer = 5; e.ApplyKnockback(ad, knk, d);
                dt.push_back(DamageText{ Vector3{e.position.x, 1.5f - ((float)hc * 0.4f), e.position.z}, dmg, 0.8f }); hc++;
            }
        }
    }
}
void Player::Draw() {
    DrawCube(position, 1, 1, 1, RED); DrawCubeWires(position, 1, 1, 1, MAROON);
    for (auto& p : projectiles) { if (p.type == 0) DrawCube(p.pos, 0.2f, 0.2f, 0.2f, BROWN); else DrawSphere(p.pos, p.radius, SKYBLUE); }
    if (isAttacking) {
        float b = atan2f(lastAimDir.z, lastAimDir.x);
        if (currentWeapon == SWORD) { for (int i = -60; i <= 60; i += 10) DrawLine3D(Vector3{ position.x, 0.15f, position.z }, Vector3{ position.x + cosf(b + i * DEG2RAD) * 4.5f, 0.15f, position.z + sinf(b + i * DEG2RAD) * 4.5f }, YELLOW); }
        else if (currentWeapon == SPEAR) { Vector3 r = Vector3{ -lastAimDir.z,0,lastAimDir.x }, p1 = Vector3Add(position, Vector3Scale(r, 1.3f)), p2 = Vector3Subtract(position, Vector3Scale(r, 1.3f)), p3 = Vector3Add(p2, Vector3Scale(lastAimDir, 8.2f)), p4 = Vector3Add(p1, Vector3Scale(lastAimDir, 8.2f)); p1.y = p2.y = p3.y = p4.y = 0.15f; DrawLine3D(p1, p2, YELLOW); DrawLine3D(p2, p3, YELLOW); DrawLine3D(p3, p4, YELLOW); DrawLine3D(p4, p1, YELLOW); }
        else if (currentWeapon == AXE) DrawSphereWires(Vector3Add(position, Vector3Scale(lastAimDir, 3.5f)), 3.5f, 8, 8, YELLOW);
    }
}