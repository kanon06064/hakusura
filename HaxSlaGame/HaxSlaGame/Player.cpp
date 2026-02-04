#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "DataManager.h"
#include "EffectManager.h"
#include "raymath.h"
#include <math.h>

Player::Player(Vector3 sp) : position(sp), speed(0.15f), radius(0.45f), attackTimer(0), isAttacking(false), lastAimDir({ 1,0,0 }), hp(100), maxHp(100), attackPower(12), defense(5), level(1), exp(0), expToNext(100), skillPoints(0) {
    activeSlot = 0; equippedData[0].id = -1; equippedData[1].id = -1;
    equippedWeapons[0] = NONE; equippedWeapons[1] = NONE;

    // 初期装備
    ItemData s1 = DataManager::GetItemConfigCopy(0);
    ItemData s2 = DataManager::GetItemConfigCopy(100);
    if (s1.id != -1) { equippedData[0] = s1; equippedWeapons[0] = (WeaponType)s1.weaponSubtype; }
    if (s2.id != -1) { equippedData[1] = s2; equippedWeapons[1] = (WeaponType)s2.weaponSubtype; }
    currentWeapon = equippedWeapons[0];
    InitSkillTree();
}

void Player::InitSkillTree() {
    skillTree.push_back({ 0, "START", {400, 350}, {}, true, 0 });
    skillTree.push_back({ 1, "ATK I", {550, 250}, {0}, false, 1, 10, 0, 0 });
    skillTree.push_back({ 2, "DEF I", {550, 450}, {0}, false, 1, 0, 5, 0 });
    skillTree.push_back({ 3, "HP I", {300, 450}, {0}, false, 1, 0, 0, 50 });
}

bool Player::IsSkillAvailable(int id) {
    if (id < 0 || id >= (int)skillTree.size() || skillTree[id].unlocked) return false;
    if (skillTree[id].reqIds.empty()) return true;
    for (int r : skillTree[id].reqIds) if (skillTree[r].unlocked) return true;
    return false;
}

void Player::UnlockSkill(int id) {
    if (IsSkillAvailable(id) && skillPoints >= skillTree[id].cost) {
        skillPoints -= skillTree[id].cost; skillTree[id].unlocked = true;
        attackPower += skillTree[id].atkAdd; defense += skillTree[id].defAdd; maxHp += skillTree[id].hpAdd; hp += skillTree[id].hpAdd;
    }
}

void Player::AddExp(int a, EffectManager& fx) { exp += a; if (exp >= expToNext) LevelUp(fx); }

void Player::LevelUp(EffectManager& fx) {
    level++; exp -= expToNext; expToNext = (int)((float)expToNext * 1.5f);
    maxHp += 20; hp = maxHp; attackPower += 2; defense += 1.5f; skillPoints += 3;
    fx.SpawnDamageText(position, 999); // 999 is level up signal
}

bool Player::AddToInventory(ItemData item) {
    if (item.type == "EQUIP") { if ((int)inventoryEquip.size() >= MAX_EQUIP_INV) return false; inventoryEquip.push_back(item); }
    else { for (auto& i : inventoryItems) if (i.id == item.id && i.count < MAX_ITEM_STACK) { i.count++; return true; } if ((int)inventoryItems.size() >= MAX_ITEM_TYPES) return false; inventoryItems.push_back(item); }
    return true;
}

void Player::UseItem(int idx) {
    if (idx < 0 || idx >= (int)inventoryItems.size()) return;
    if (inventoryItems[idx].type == "CONSUMABLE") { hp = fminf(maxHp, hp + inventoryItems[idx].heal); inventoryItems[idx].count--; if (inventoryItems[idx].count <= 0) inventoryItems.erase(inventoryItems.begin() + idx); }
}

void Player::EquipWeapon(int invIdx, int slot) {
    if (invIdx < 0 || invIdx >= (int)inventoryEquip.size()) return;
    if (equippedData[slot].id != -1) inventoryEquip.push_back(equippedData[slot]);
    equippedData[slot] = inventoryEquip[invIdx]; equippedWeapons[slot] = (WeaponType)equippedData[slot].weaponSubtype;
    inventoryEquip.erase(inventoryEquip.begin() + invIdx);
    if (activeSlot == slot) currentWeapon = equippedWeapons[slot];
}

void Player::UnequipWeapon(int slot) {
    if (equippedData[slot].id == -1 || (int)inventoryEquip.size() >= MAX_EQUIP_INV) return;
    inventoryEquip.push_back(equippedData[slot]);
    equippedData[slot] = ItemData(); equippedWeapons[slot] = NONE;
    if (activeSlot == slot) currentWeapon = NONE;
}

void Player::Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop) {
    if (stop) return;
    if (IsKeyPressed(KEY_Q)) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }

    // 移動処理
    Vector3 cf = Vector3Normalize(Vector3Subtract(cam.target, cam.position)); cf.y = 0; cf = Vector3Normalize(cf);
    Vector3 cr = { -cf.z, 0, cf.x }, md = { 0,0,0 };
    if (IsKeyDown(KEY_W)) md = Vector3Add(md, cf); if (IsKeyDown(KEY_S)) md = Vector3Subtract(md, cf);
    if (IsKeyDown(KEY_A)) md = Vector3Subtract(md, cr); if (IsKeyDown(KEY_D)) md = Vector3Add(md, cr);

    if (Vector3Length(md) > 0) {
        md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, speed);
        if (!d.CheckCollisionRadius(Vector3Add(position, { v.x, 0, 0 }), radius)) position.x += v.x;
        if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, v.z }), radius)) position.z += v.z;
    }

    // エイム計算
    Ray ray = GetMouseRay(GetMousePosition(), cam);
    // プレイヤーの高さ平面との交差判定
    float groundHeight = position.y;
    // ray.position + ray.direction * t = y -> t = (y - ray.pos.y) / ray.dir.y
    if (ray.direction.y != 0) {
        float t = (groundHeight - ray.position.y) / ray.direction.y;
        Vector3 targetPos = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        lastAimDir = Vector3Normalize(Vector3Subtract(targetPos, position));
        lastAimDir.y = 0; // 水平化
        lastAimDir = Vector3Normalize(lastAimDir);
    }

    if (attackTimer > 0) attackTimer -= GetFrameTime();

    if (IsMouseButtonPressed(0) && attackTimer <= 0 && currentWeapon != NONE) {
        PerformAttack(lastAimDir, enemies, d, fx);
        attackTimer = (currentWeapon >= BOW) ? 0.35f : 0.5f;
        isAttacking = true;
    }
}

void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
    Vector3 attackOrigin = Vector3Add(position, { 0, 0.8f, 0 });

    if (currentWeapon == BOW) {
        fx.SpawnProjectile(attackOrigin, ad, 25.0f, 0, true);
    }
    else if (currentWeapon == WAND) {
        fx.SpawnProjectile(attackOrigin, ad, 15.0f, 1, true);
    }
    else {
        // 近接攻撃エフェクト
        EffectType type = FX_SLASH;
        if (currentWeapon == SPEAR) type = FX_THRUST;
        else if (currentWeapon == AXE) type = FX_SMASH;

        fx.SpawnEffect(attackOrigin, ad, type, SKYBLUE);

        // 近接ヒット判定
        for (auto& e : enemies) {
            if (!d.HasLineOfSight(position, e.position)) continue;
            Vector3 v = Vector3Subtract(e.position, position);
            float dist = Vector3Length(v);
            bool hit = false;
            float knk = 0;

            switch (currentWeapon) {
            case SWORD:
                if (dist < 4.5f && Vector3DotProduct(ad, Vector3Normalize(v)) > cosf(60 * DEG2RAD)) {
                    hit = true; knk = 0.8f;
                }
                break;
            case SPEAR: {
                float f = Vector3DotProduct(v, ad);
                Vector3 side = { -ad.z, 0, ad.x };
                float s = fabsf(Vector3DotProduct(v, side));
                if (f > 0 && f < 8.2f && s < 1.5f) { hit = true; knk = 2.2f; }
            } break;
            case AXE:
                if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(ad, 3.5f))) < 3.5f) {
                    hit = true; knk = 1.3f;
                }
                break;
            default: break;
            }

            if (hit) {
                int dmg = (int)((attackPower + equippedData[activeSlot].atkBonus)) + GetRandomValue(-2, 3);
                e.hp -= (float)dmg;
                e.hudTimer = 5;
                e.ApplyKnockback(ad, knk, d);

                fx.SpawnDamageText(e.position, dmg);
                fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, ORANGE);
            }
        }
    }
}

void Player::Draw(bool debug) {
    DrawCube(position, 1, 1, 1, RED); DrawCubeWires(position, 1, 1, 1, MAROON);
    if (currentWeapon != NONE) {
        // エイムガイド
        DrawLine3D(position, Vector3Add(position, Vector3Scale(lastAimDir, 3.0f)), Fade(YELLOW, 0.3f));
    }
}