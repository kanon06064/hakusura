#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "DataManager.h"
#include "EffectManager.h"
#include "raymath.h"
#include <math.h>

Player::Player(Vector3 sp) : position(sp), baseSpeed(0.15f), radius(0.45f), attackTimer(0), isAttacking(false), lastAimDir({ 1,0,0 }), hp(100), maxHp(100), attackPower(12), defense(5), level(1), exp(0), expToNext(100), skillPoints(0), gold(0) {
    speed = baseSpeed;
    activeSlot = 0; equippedData[0].id = -1; equippedData[1].id = -1;
    equippedWeapons[0] = NONE; equippedWeapons[1] = NONE;

    // スキルタイマー初期化
    dashTimer = 0; dashCooldownTimer = 0;
    smashCooldownTimer = 0;
    stealthTimer = 0; stealthCooldownTimer = 0;
    isStealth = false;

    ItemData s1 = DataManager::GetItemConfigCopy(0);
    ItemData s2 = DataManager::GetItemConfigCopy(100);
    if (s1.id != -1) { equippedData[0] = s1; equippedWeapons[0] = (WeaponType)s1.weaponSubtype; }
    if (s2.id != -1) { equippedData[1] = s2; equippedWeapons[1] = (WeaponType)s2.weaponSubtype; }
    currentWeapon = equippedWeapons[0];
    InitSkillTree();
}

std::string Player::GetFullItemName(const ItemData& item) {
    if (item.id == -1) return "EMPTY";
    Modifier mod = DataManager::GetModifier(item.modifierId);
    if (mod.name.empty()) return item.name;
    return mod.name + " " + item.name;
}

float Player::GetItemTotalAtkBonus(const ItemData& item) {
    if (item.id == -1) return 0.0f;
    return item.atkBonus + DataManager::GetModifier(item.modifierId).atkBonusAdd;
}

void Player::InitSkillTree() {
    // UI座標は 1280x720 の画面を想定して配置
    // 中央下からスタート
    skillTree.push_back({ 0, "START", {640, 650}, {}, true, 0 });

    // --- 左ルート: 防御 & ステルス ---
    skillTree.push_back({ 1, "HP I",  {500, 550}, {0}, false, 1, 0, 0, 30, SKILL_PASSIVE });
    skillTree.push_back({ 2, "DEF I", {400, 480}, {1}, false, 1, 0, 3, 0, SKILL_PASSIVE });
    skillTree.push_back({ 3, "ステルス", {300, 400}, {2}, false, 3, 0, 0, 0, SKILL_ACTIVE_STEALTH, 15.0f }); // CD 15秒
    skillTree.push_back({ 4, "HP II", {300, 300}, {3}, false, 2, 0, 0, 50, SKILL_PASSIVE });

    // --- 中央ルート: 攻撃 & 強撃 ---
    skillTree.push_back({ 5, "ATK I", {640, 500}, {0}, false, 1, 5, 0, 0, SKILL_PASSIVE });
    skillTree.push_back({ 6, "強撃",   {640, 400}, {5}, false, 2, 0, 0, 0, SKILL_ACTIVE_SMASH, 5.0f }); // CD 5秒
    skillTree.push_back({ 7, "ATK II", {640, 300}, {6}, false, 3, 10, 0, 0, SKILL_PASSIVE });
    skillTree.push_back({ 8, "ATK III",{640, 200}, {7}, false, 5, 20, 0, 0, SKILL_PASSIVE });

    // --- 右ルート: 移動 & ダッシュ ---
    skillTree.push_back({ 9,  "SPD I", {780, 550}, {0}, false, 1, 0, 0, 0, SKILL_PASSIVE }); // 今回速度UPパッシブは未実装だがノードとして配置
    skillTree.push_back({ 10, "ダッシュ", {880, 480}, {9}, false, 2, 0, 0, 0, SKILL_ACTIVE_DASH, 3.0f }); // CD 3秒
    skillTree.push_back({ 11, "DEF II", {980, 400}, {10}, false, 2, 0, 5, 0, SKILL_PASSIVE });
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
        // パッシブ効果の即時適用
        attackPower += skillTree[id].atkAdd;
        defense += skillTree[id].defAdd;
        maxHp += skillTree[id].hpAdd;
        hp += skillTree[id].hpAdd;
    }
}

// スキル解放状態チェック
bool Player::IsSkillUnlocked(SkillType type) {
    for (auto& node : skillTree) {
        if (node.type == type && node.unlocked) return true;
    }
    return false;
}

// クールダウンタイマー取得（UI表示用）
float Player::GetSkillCooldown(SkillType type) {
    switch (type) {
    case SKILL_ACTIVE_DASH: return dashCooldownTimer;
    case SKILL_ACTIVE_SMASH: return smashCooldownTimer;
    case SKILL_ACTIVE_STEALTH: return stealthCooldownTimer;
    default: return 0.0f;
    }
}

// 最大クールダウン取得
float Player::GetSkillMaxCooldown(SkillType type) {
    for (auto& node : skillTree) {
        if (node.type == type) return node.maxCooldown;
    }
    return 1.0f;
}

void Player::AddExp(int a, EffectManager& fx) { exp += a; if (exp >= expToNext) LevelUp(fx); }

void Player::LevelUp(EffectManager& fx) {
    level++; exp -= expToNext; expToNext = (int)((float)expToNext * 1.5f);
    maxHp += 20; hp = maxHp; attackPower += 2; defense += 1.5f; skillPoints += 3;
    fx.SpawnDamageText(position, 999);
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

    // クールダウン更新
    float dt = GetFrameTime();
    if (dashCooldownTimer > 0) dashCooldownTimer -= dt;
    if (smashCooldownTimer > 0) smashCooldownTimer -= dt;
    if (stealthCooldownTimer > 0) stealthCooldownTimer -= dt;

    // スキル効果時間更新
    if (dashTimer > 0) dashTimer -= dt;
    if (stealthTimer > 0) stealthTimer -= dt; else isStealth = false;

    // --- スキル入力 ---
    // [1] ダッシュ
    if (IsKeyPressed(KEY_ONE) && IsSkillUnlocked(SKILL_ACTIVE_DASH) && dashCooldownTimer <= 0) {
        dashTimer = 0.2f; // ダッシュ時間
        dashCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_DASH);
        fx.SpawnEffect(position, { 0,1,0 }, FX_HIT, WHITE); // エフェクト
    }
    // [2] 強撃
    if (IsKeyPressed(KEY_TWO) && IsSkillUnlocked(SKILL_ACTIVE_SMASH) && smashCooldownTimer <= 0 && currentWeapon != NONE && attackTimer <= 0) {
        // 強撃発動
        Ray ray = GetMouseRay(GetMousePosition(), cam);
        if (ray.direction.y != 0) {
            float t = (position.y - ray.position.y) / ray.direction.y;
            Vector3 targetPos = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            lastAimDir = Vector3Normalize(Vector3Subtract(targetPos, position));
            lastAimDir.y = 0; lastAimDir = Vector3Normalize(lastAimDir);
        }
        PerformSmash(lastAimDir, enemies, d, fx);
        smashCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_SMASH);
        attackTimer = 0.8f; // 硬直
    }
    // [3] ステルス
    if (IsKeyPressed(KEY_THREE) && IsSkillUnlocked(SKILL_ACTIVE_STEALTH) && stealthCooldownTimer <= 0) {
        isStealth = true;
        stealthTimer = 10.0f; // 10秒間透明
        stealthCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_STEALTH);
        fx.SpawnEffect(position, { 0,1,0 }, FX_HIT, BLUE);
    }

    // 武器切り替え
    if (IsKeyPressed(KEY_Q)) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }

    // 移動処理
    Vector3 cf = Vector3Normalize(Vector3Subtract(cam.target, cam.position)); cf.y = 0; cf = Vector3Normalize(cf);
    Vector3 cr = { -cf.z, 0, cf.x }, md = { 0,0,0 };
    if (IsKeyDown(KEY_W)) md = Vector3Add(md, cf); if (IsKeyDown(KEY_S)) md = Vector3Subtract(md, cf);
    if (IsKeyDown(KEY_A)) md = Vector3Subtract(md, cr); if (IsKeyDown(KEY_D)) md = Vector3Add(md, cr);

    // ダッシュ中は速度3倍
    float currentSpeed = (dashTimer > 0) ? baseSpeed * 3.0f : baseSpeed;

    if (Vector3Length(md) > 0) {
        md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, currentSpeed);
        if (!d.CheckCollisionRadius(Vector3Add(position, { v.x, 0, 0 }), radius)) position.x += v.x;
        if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, v.z }), radius)) position.z += v.z;
    }

    // エイム計算
    Ray ray = GetMouseRay(GetMousePosition(), cam);
    if (ray.direction.y != 0) {
        float t = (position.y - ray.position.y) / ray.direction.y;
        Vector3 targetPos = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        lastAimDir = Vector3Normalize(Vector3Subtract(targetPos, position));
        lastAimDir.y = 0;
        lastAimDir = Vector3Normalize(lastAimDir);
    }

    if (attackTimer > 0) attackTimer -= GetFrameTime();

    // 通常攻撃
    if (IsMouseButtonPressed(0) && attackTimer <= 0 && currentWeapon != NONE) {
        PerformAttack(lastAimDir, enemies, d, fx);
        attackTimer = (currentWeapon >= BOW) ? 0.35f : 0.5f;
        isAttacking = true;
    }
}

void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
    Vector3 attackOrigin = Vector3Add(position, { 0, 0.8f, 0 });

    if (currentWeapon == BOW || currentWeapon == WAND) {
        float speed = (currentWeapon == BOW) ? 25.0f : 15.0f;
        int type = (currentWeapon == BOW) ? 0 : 1;
        fx.SpawnProjectile(attackOrigin, ad, speed, type, true);
    }
    else {
        EffectType type = FX_SLASH;
        if (currentWeapon == SPEAR) type = FX_THRUST;
        else if (currentWeapon == AXE) type = FX_SMASH;

        fx.SpawnEffect(attackOrigin, ad, type, SKYBLUE);

        for (auto& e : enemies) {
            if (!d.HasLineOfSight(position, e.position)) continue;
            Vector3 v = Vector3Subtract(e.position, position);
            float dist = Vector3Length(v);
            bool hit = false;
            float knk = 0;

            switch (currentWeapon) {
            case SWORD: if (dist < 4.5f && Vector3DotProduct(ad, Vector3Normalize(v)) > cosf(60 * DEG2RAD)) { hit = true; knk = 0.8f; } break;
            case SPEAR: { float f = Vector3DotProduct(v, ad); Vector3 side = { -ad.z, 0, ad.x }; float s = fabsf(Vector3DotProduct(v, side)); if (f > 0 && f < 8.2f && s < 1.5f) { hit = true; knk = 2.2f; } } break;
            case AXE: if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(ad, 3.5f))) < 3.5f) { hit = true; knk = 1.3f; } break;
            default: break;
            }

            if (hit) {
                float totalBonus = GetItemTotalAtkBonus(equippedData[activeSlot]);
                int dmg = (int)((attackPower + totalBonus)) + GetRandomValue(-2, 3);
                e.hp -= (float)dmg;
                e.hudTimer = 5;
                e.ApplyKnockback(ad, knk, d);
                fx.SpawnDamageText(e.position, dmg);
                fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, ORANGE);
                // 攻撃を当てたらステルス解除
                isStealth = false; stealthTimer = 0;
            }
        }
    }
}

// 【追加】強撃スキル
void Player::PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
    Vector3 attackOrigin = Vector3Add(position, { 0, 0.8f, 0 });
    fx.SpawnEffect(attackOrigin, ad, FX_SMASH, RED); // 派手なエフェクト

    for (auto& e : enemies) {
        if (!d.HasLineOfSight(position, e.position)) continue;

        // 斧のような範囲攻撃判定（広め）
        if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(ad, 3.5f))) < 4.5f) {
            float totalBonus = GetItemTotalAtkBonus(equippedData[activeSlot]);
            // ダメージ2.5倍
            int dmg = (int)((attackPower + totalBonus) * 2.5f) + GetRandomValue(5, 10);
            e.hp -= (float)dmg;
            e.hudTimer = 5;
            e.ApplyKnockback(ad, 3.0f, d); // 強いノックバック
            fx.SpawnDamageText(e.position, dmg);
            fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, PURPLE);

            isStealth = false; stealthTimer = 0;
        }
    }
}

void Player::Draw(bool debug) {
    Color bodyColor = RED;
    if (isStealth) bodyColor = Fade(BLUE, 0.3f); // ステルス中は半透明の青

    DrawCube(position, 1, 1, 1, bodyColor);
    DrawCubeWires(position, 1, 1, 1, MAROON);
    if (currentWeapon != NONE) {
        DrawLine3D(position, Vector3Add(position, Vector3Scale(lastAimDir, 3.0f)), Fade(YELLOW, 0.3f));
    }
}