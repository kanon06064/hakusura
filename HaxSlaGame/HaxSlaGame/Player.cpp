#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "DataManager.h"
#include "EffectManager.h"
#include "AudioManager.h"
#include "UI.h" 
#include "raymath.h"
#include <math.h>
#include <algorithm>
#include <string>
#include <cctype>

// 変数の実体定義
Vector3 Player::customWeaponOffsetPos = { -10.0f, 0.0f, 0.0f };
Vector3 Player::customWeaponOffsetRot = { 90.0f, 180.0f, 0.0f };
float Player::customWeaponScale = 100.0f;

static std::string T(const std::string& key, const std::string& def) {
    if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
    return def;
}

// ボーンのスケールを復活させ、数学的に正しい順序でグローバル行列を生成
static Matrix GetPlayerBoneGlobalMatrix(Model model, ModelAnimation anim, int frame, int boneIndex) {
    if (boneIndex < 0 || boneIndex >= model.boneCount) return MatrixIdentity();
    if (anim.frameCount <= 0 || anim.framePoses == nullptr) return MatrixIdentity();

    if (frame < 0) frame = 0;
    if (frame >= anim.frameCount) frame = anim.frameCount - 1;

    // RaylibのIQMアニメーション(framePoses)には既にルートからの絶対座標が格納されています。
    // 親ボーンを辿って乗算すると二重適用になり座標が彼方へ飛んでしまうため、単一ボーンを取り出します。
    Transform b = anim.framePoses[frame][boneIndex];

    // 引継ぎ書の指示にある「スケール1.0（等倍）を維持した純粋な手の動きの行列」を作成するため、
    // ボーン自体のスケール(b.scale)を除外して合成します。
    Matrix mat = MatrixMultiply(
        QuaternionToMatrix(b.rotation),
        MatrixTranslate(b.translation.x, b.translation.y, b.translation.z)
    );

    return mat;
}
Player::Player(Vector3 sp) : position(sp), baseSpeed(0.18f), radius(0.45f), attackTimer(0), isAttacking(false),
lastAimDir({ 1,0,0 }), hp(100), maxHp(100), attackPower(12), defense(5), level(1), exp(0),
expToNext(100), skillPoints(0), gold(0)
{
    speed = baseSpeed;
    activeSlot = 0;
    equippedData[0].id = -1; equippedData[1].id = -1;
    equippedWeapons[0] = NONE; equippedWeapons[1] = NONE;
    for (int i = 0; i < 5; i++) equippedArmor[i].id = -1;

    dashTimer = 0; dashCooldownTimer = 0;
    smashCooldownTimer = 0;
    stealthTimer = 0; stealthCooldownTimer = 0;
    kongoTimer = 0; kongoCooldownTimer = 0;
    zoukyouTimer = 0; zoukyouCooldownTimer = 0;
    healCooldownTimer = 0;
    cooldownReduction = 0; healBonus = 0;
    isStealth = false;

    animTime = 0.0f;
    currentAnimIndex = 4;
    prevAnimIndex = 4;
    modelRotation = 0.0f;
    isDead = false;

    ItemData s1 = DataManager::GetItemConfigCopy(0);
    if (s1.id != -1) { equippedData[0] = s1; equippedWeapons[0] = (WeaponType)s1.weaponSubtype; }
    currentWeapon = equippedWeapons[0];

    InitSkillTree();
    RecalculateStats();
}

std::string Player::GetFullItemName(const ItemData& item) {
    if (item.id == -1) return "EMPTY";
    Modifier mod = DataManager::GetModifier(item.modifierId);
    return mod.name.empty() ? item.name : mod.name + " " + item.name;
}

float Player::GetItemTotalAtkBonus(const ItemData& item) {
    if (item.id == -1) return 0.0f;
    return item.atkBonus + DataManager::GetModifier(item.modifierId).atk;
}

Color Player::GetItemRarityColor(const ItemData& item) {
    if (item.id == -1) return DARKGRAY;
    if (item.type == "MATERIAL") return LIGHTGRAY;
    if (item.type == "CONSUMABLE") return LIME;
    int tier = 1;
    if (item.id >= 100 && item.id < 200) tier = 1;
    else if (item.id >= 200 && item.id < 300) tier = 2;
    else if (item.id >= 300 && item.id < 400) tier = 3;
    else if (item.id >= 400 && item.id < 500) tier = 4;
    else if (item.id >= 500) tier = 5;
    if (item.modifierId != 0 && tier < 5) tier++;
    switch (tier) {
    case 1: return WHITE; case 2: return GREEN; case 3: return SKYBLUE;
    case 4: return PURPLE; case 5: return GOLD; default: return WHITE;
    }
}

void Player::RecalculateStats() {
    float bHp = 100.0f + (level - 1) * 20.0f;
    float bAtk = 12.0f + (level - 1) * 2.0f;
    float bDef = 5.0f + (level - 1) * 1.5f;
    float bSpd = 0.18f;
    cooldownReduction = 0.0f;
    healBonus = 0.0f;

    for (const auto& node : skillTree) if (node.unlocked) {
        bAtk += node.atkAdd;
        bDef += node.defAdd;
        bHp += node.hpAdd;
        cooldownReduction += node.cdRedAdd;
        healBonus += node.healAdd;
    }

    if (zoukyouTimer > 0) bAtk *= 1.5f;
    if (kongoTimer > 0) bDef += 20.0f;

    for (int i = 0; i < 5; i++) if (equippedArmor[i].id != -1) {
        Modifier m = DataManager::GetModifier(equippedArmor[i].modifierId);
        bHp += equippedArmor[i].hpBonus + m.hp; bDef += equippedArmor[i].defBonus + m.def;
        bAtk += equippedArmor[i].atkBonus + m.atk; bSpd += equippedArmor[i].speedBonus + m.spd;
    }
    maxHp = bHp; attackPower = bAtk; defense = bDef; baseSpeed = bSpd;
    if (hp > maxHp) hp = maxHp;
}

void Player::InitSkillTree() {
    skillTree.clear();
    skillTree.push_back({ 0, T("SKILL_NAME_START", "START"), {600, 400}, {}, true, 0, 0,0,0,0,0, T("SKILL_DESC_START", "Skill Tree Start"), SKILL_PASSIVE });
    skillTree.push_back({ 1, T("SKILL_NAME_ATK1", "ATK I"),  {600, 300}, {0}, false, 1, 3.0f, 0, 0, 0, 0, T("SKILL_DESC_ATK1", "ATK +3"), SKILL_PASSIVE });
    skillTree.push_back({ 2, T("SKILL_NAME_ATK2", "ATK II"), {600, 200}, {1}, false, 2, 5.0f, 0, 0, 0, 0, T("SKILL_DESC_ATK2", "ATK +5"), SKILL_PASSIVE });
    skillTree.push_back({ 3, T("SMASH", "SMASH"), {600, 100}, {2}, false, 3, 0, 0, 0, 0, 0, T("SKILL_DESC_SMASH", "Active: Deal heavy damage & knockback"), SKILL_ACTIVE_SMASH, 8.0f });

    skillTree.push_back({ 4, T("SKILL_NAME_DEF1", "DEF I"),  {695, 331}, {0}, false, 1, 0, 2.0f, 0, 0, 0, T("SKILL_DESC_DEF1", "DEF +2"), SKILL_PASSIVE });
    skillTree.push_back({ 5, T("SKILL_NAME_DEF2", "DEF II"), {790, 262}, {4}, false, 2, 0, 3.0f, 0, 0, 0, T("SKILL_DESC_DEF2", "DEF +3"), SKILL_PASSIVE });
    skillTree.push_back({ 6, T("KONGO", "KONGO"), {885, 193}, {5}, false, 3, 0, 0, 0, 0, 0, T("SKILL_DESC_KONGO", "Active: Boost DEF temporarily"), SKILL_ACTIVE_KONGO, 15.0f });

    skillTree.push_back({ 7, T("SKILL_NAME_HP1", "HP I"),   {659, 481}, {0}, false, 1, 0, 0, 20.0f, 0, 0, T("SKILL_DESC_HP1", "HP +20"), SKILL_PASSIVE });
    skillTree.push_back({ 8, T("SKILL_NAME_HP2", "HP II"),  {718, 562}, {7}, false, 2, 0, 0, 30.0f, 0, 0, T("SKILL_DESC_HP2", "HP +30"), SKILL_PASSIVE });
    skillTree.push_back({ 9, T("ZOUKYOU", "ZOUKYOU"), {777, 643},{8}, false, 3, 0, 0, 0, 0, 0, T("SKILL_DESC_ZOUKYOU", "Active: Boost ATK temporarily"), SKILL_ACTIVE_ZOUKYOU, 20.0f });

    skillTree.push_back({ 10, T("SKILL_NAME_CD1", "CD I"),  {541, 481}, {0}, false, 1, 0, 0, 0, 0.05f, 0, T("SKILL_DESC_CD1", "Cooldown -5%"), SKILL_PASSIVE });
    skillTree.push_back({ 11, T("SKILL_NAME_CD2", "CD II"), {482, 562}, {10}, false, 2, 0, 0, 0, 0.10f, 0, T("SKILL_DESC_CD2", "Cooldown -10%"), SKILL_PASSIVE });
    skillTree.push_back({ 12, T("STEALTH", "STEALTH"), {423, 643},{11}, false, 3, 0, 0, 0, 0, 0, T("SKILL_DESC_STEALTH", "Active: Become undetectable"), SKILL_ACTIVE_STEALTH, 15.0f });

    skillTree.push_back({ 13, T("SKILL_NAME_HEAL1", "HEAL I"), {505, 331}, {0}, false, 1, 0, 0, 0, 0, 10.0f, T("SKILL_DESC_HEAL1", "Healing +10"), SKILL_PASSIVE });
    skillTree.push_back({ 14, T("SKILL_NAME_HEAL2", "HEAL II"),{410, 262},{13}, false, 2, 0, 0, 0, 0, 20.0f, T("SKILL_DESC_HEAL2", "Healing +20"), SKILL_PASSIVE });
    skillTree.push_back({ 15, T("HEAL", "HEAL"), {315, 193},{14}, false, 3, 0, 0, 0, 0, 0, T("SKILL_DESC_HEAL", "Active: Restore HP instantly"), SKILL_ACTIVE_HEAL, 25.0f });

    skillTree.push_back({ 16, T("DASH", "DASH"), {700, 400}, {0}, false, 1, 0, 0, 0, 0, 0, T("SKILL_DESC_DASH", "Active: Quick dodge"), SKILL_ACTIVE_DASH, 3.0f });
}

bool Player::IsSkillAvailable(int id) {
    for (auto& n : skillTree) if (n.id == id) { if (n.unlocked) return false; if (n.reqIds.empty()) return true; for (int r : n.reqIds) for (auto& rn : skillTree) if (rn.id == r && rn.unlocked) return true; }
    return false;
}

void Player::UnlockSkill(int id) {
    if (IsSkillAvailable(id)) { for (auto& n : skillTree) if (n.id == id && skillPoints >= n.cost) { skillPoints -= n.cost; n.unlocked = true; AudioManager::PlaySE(SE_SKILL); RecalculateStats(); return; } }
}

bool Player::IsSkillUnlocked(SkillType t) { for (auto& n : skillTree) if (n.type == t && n.unlocked) return true; return false; }

float Player::GetSkillCooldown(SkillType t) {
    if (t == SKILL_ACTIVE_DASH) return dashCooldownTimer;
    if (t == SKILL_ACTIVE_SMASH) return smashCooldownTimer;
    if (t == SKILL_ACTIVE_STEALTH) return stealthCooldownTimer;
    if (t == SKILL_ACTIVE_KONGO) return kongoCooldownTimer;
    if (t == SKILL_ACTIVE_ZOUKYOU) return zoukyouCooldownTimer;
    if (t == SKILL_ACTIVE_HEAL) return healCooldownTimer;
    return 0.0f;
}

float Player::GetSkillMaxCooldown(SkillType t) { for (auto& n : skillTree) if (n.type == t) return n.maxCooldown; return 1.0f; }

void Player::AddExp(int a, EffectManager& fx) { exp += a; while (exp >= expToNext) LevelUp(fx); }
void Player::LevelUp(EffectManager& fx) {
    level++; exp -= expToNext; expToNext = (int)(expToNext * 1.5f); skillPoints += 3;
    fx.SpawnDamageText(position, 999); AudioManager::PlaySE(SE_LEVELUP);
    RecalculateStats(); hp = maxHp;
    UI::AddSystemLog(T("LOG_LEVEL_UP", "LEVEL UP!"), YELLOW);
}

bool Player::AddToInventory(ItemData item) {
    if (item.type == "EQUIP" || item.type == "ARMOR") { if (inventoryEquip.size() >= MAX_EQUIP_INV) return false; inventoryEquip.push_back(item); }
    else { for (auto& i : inventoryItems) if (i.id == item.id && i.count < MAX_ITEM_STACK) { i.count++; return true; } if (inventoryItems.size() >= MAX_ITEM_TYPES) return false; inventoryItems.push_back(item); }
    return true;
}

void Player::UseItem(int idx) { if (idx >= 0 && idx < (int)inventoryItems.size() && inventoryItems[idx].type == "CONSUMABLE") { hp = fminf(maxHp, hp + inventoryItems[idx].heal); if (--inventoryItems[idx].count <= 0) inventoryItems.erase(inventoryItems.begin() + idx); AudioManager::PlaySE(SE_HEAL); } }

void Player::EquipWeapon(int invIdx, int slot) {
    if (invIdx < 0 || invIdx >= (int)inventoryEquip.size()) return;
    if (equippedData[slot].id != -1) inventoryEquip.push_back(equippedData[slot]);
    equippedData[slot] = inventoryEquip[invIdx]; equippedWeapons[slot] = (WeaponType)equippedData[slot].weaponSubtype;
    inventoryEquip.erase(inventoryEquip.begin() + invIdx); if (activeSlot == slot) currentWeapon = equippedWeapons[slot];
    AudioManager::PlaySE(SE_CLICK); RecalculateStats();
}

void Player::UnequipWeapon(int slot) { if (equippedData[slot].id == -1) return; inventoryEquip.push_back(equippedData[slot]); equippedData[slot] = ItemData(); equippedWeapons[slot] = NONE; if (activeSlot == slot) currentWeapon = NONE; AudioManager::PlaySE(SE_CLICK); RecalculateStats(); }
void Player::EquipArmor(int invIdx, int slot) { if (invIdx < 0 || invIdx >= (int)inventoryEquip.size()) return; if (equippedArmor[slot].id != -1) inventoryEquip.push_back(equippedArmor[slot]); equippedArmor[slot] = inventoryEquip[invIdx]; inventoryEquip.erase(inventoryEquip.begin() + invIdx); AudioManager::PlaySE(SE_CLICK); RecalculateStats(); }
void Player::UnequipArmor(int slot) { if (equippedArmor[slot].id == -1) return; inventoryEquip.push_back(equippedArmor[slot]); equippedArmor[slot] = ItemData(); AudioManager::PlaySE(SE_CLICK); RecalculateStats(); }

void Player::UpdateHuntQuest(int eId) { for (auto& q : activeQuests) if (!q.isCompleted) { QuestData d = DataManager::GetQuestData(q.questId); if (d.type == QUEST_HUNT && d.targetId == eId) if (++q.currentCount >= d.targetCount) q.isCompleted = true; } }
bool Player::CheckGatherQuest(int itemId, int count) { int sum = 0; for (auto& i : inventoryItems) if (i.id == itemId) sum += i.count; return sum >= count; }
void Player::CompleteQuest(int qId) { for (auto it = activeQuests.begin(); it != activeQuests.end(); ++it) if (it->questId == qId) { QuestData d = DataManager::GetQuestData(qId); gold += d.rewardGold; if (d.rewardItemId != -1) { ItemData r = DataManager::GetItemConfigCopy(d.rewardItemId); r.count = d.rewardItemCount; AddToInventory(r); } clearedQuests.push_back(qId); activeQuests.erase(it); AudioManager::PlaySE(SE_REFORGE); return; } }

void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
    AudioManager::PlaySE(SE_ATTACK);
    Vector3 origin = Vector3Add(position, { 0, 0.8f, 0 });
    if (currentWeapon == WAND) fx.SpawnProjectile(origin, ad, 15.0f, 1, true);
    else {
        EffectType type = (currentWeapon == SPEAR) ? FX_THRUST : (currentWeapon == AXE ? FX_SMASH : FX_SLASH);
        fx.SpawnEffect(origin, ad, type, SKYBLUE);
        for (auto& e : enemies) if (Vector3Distance(e.position, position) < 4.5f) {
            int dmg = (int)(attackPower + GetItemTotalAtkBonus(equippedData[activeSlot]));
            e.hp -= dmg; e.ApplyKnockback(ad, 1.0f, d); fx.SpawnDamageText(e.position, dmg);
            e.hudTimer = 5.0f;
            isStealth = false;
        }
    }
}

void Player::PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
    AudioManager::PlaySE(SE_SKILL);
    fx.SpawnEffect(Vector3Add(position, { 0, 0.8f, 0 }), ad, FX_SMASH, RED);
    for (auto& e : enemies) if (Vector3Distance(e.position, position) < 5.0f) {
        int dmg = (int)(attackPower * 2.5f); e.hp -= dmg; e.ApplyKnockback(ad, 3.0f, d); fx.SpawnDamageText(e.position, dmg);
        e.hudTimer = 5.0f;
        isStealth = false;
    }
}

void Player::Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop) {
    if (stop) return;
    float dt = GetFrameTime();

    if (dashCooldownTimer > 0) dashCooldownTimer -= dt;
    if (smashCooldownTimer > 0) smashCooldownTimer -= dt;
    if (stealthCooldownTimer > 0) stealthCooldownTimer -= dt;
    if (kongoCooldownTimer > 0) kongoCooldownTimer -= dt;
    if (zoukyouCooldownTimer > 0) zoukyouCooldownTimer -= dt;
    if (healCooldownTimer > 0) healCooldownTimer -= dt;

    if (dashTimer > 0) dashTimer -= dt;
    if (stealthTimer > 0) stealthTimer -= dt; else isStealth = false;
    if (kongoTimer > 0) { kongoTimer -= dt; if (kongoTimer <= 0) RecalculateStats(); }
    if (zoukyouTimer > 0) { zoukyouTimer -= dt; if (zoukyouTimer <= 0) RecalculateStats(); }

    Vector3 cf = Vector3Normalize(Vector3Subtract(cam.target, cam.position)); cf.y = 0; cf = Vector3Normalize(cf);
    Vector3 cr = { -cf.z, 0, cf.x }, md = { 0,0,0 };

    if (IsKeyDown(DataManager::keyConfig.moveForward)) md = Vector3Add(md, cf);
    if (IsKeyDown(DataManager::keyConfig.moveBackward)) md = Vector3Subtract(md, cf);
    if (IsKeyDown(DataManager::keyConfig.moveLeft)) md = Vector3Subtract(md, cr);
    if (IsKeyDown(DataManager::keyConfig.moveRight)) md = Vector3Add(md, cr);

    if (IsGamepadAvailable(0)) {
        float axisX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float axisY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabs(axisX) > 0.2f) md = Vector3Add(md, Vector3Scale(cr, axisX));
        if (fabs(axisY) > 0.2f) md = Vector3Add(md, Vector3Scale(cf, -axisY));
    }

    bool isMoving = (Vector3Length(md) > 0.1f);
    float curSpd = (dashTimer > 0) ? baseSpeed * 2.8f : baseSpeed;

    if (isMoving) {
        md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, curSpd);
        if (!d.CheckCollisionRadius(Vector3Add(position, { v.x,0,0 }), radius)) position.x += v.x;
        if (!d.CheckCollisionRadius(Vector3Add(position, { 0,0,v.z }), radius)) position.z += v.z;
        modelRotation = atan2f(md.x, md.z) * RAD2DEG;
    }

    bool usingGamepadAim = false;
    Vector2 mouseDelta = GetMouseDelta();
    if (fabs(mouseDelta.x) > 1.0f || fabs(mouseDelta.y) > 1.0f || IsMouseButtonPressed(0) || IsMouseButtonPressed(1)) {
        usingGamepadAim = false;
    }

    if (IsGamepadAvailable(0)) {
        float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
        float ry = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
        float lx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float ly = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        bool padAttack = IsGamepadButtonDown(0, DataManager::keyConfig.padAttack);
        if (fabs(rx) > 0.2f || fabs(ry) > 0.2f || fabs(lx) > 0.2f || fabs(ly) > 0.2f || padAttack) {
            usingGamepadAim = true;
        }
    }

    if (!usingGamepadAim) {
        Ray ray = GetMouseRay(GetMousePosition(), cam);
        if (ray.direction.y != 0) {
            float t = (position.y - ray.position.y) / ray.direction.y;
            Vector3 tp = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
            lastAimDir = Vector3Normalize(Vector3Subtract(tp, position)); lastAimDir.y = 0;
        }
    }
    else {
        if (IsGamepadAvailable(0)) {
            float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
            float ry = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
            if (fabs(rx) > 0.2f || fabs(ry) > 0.2f) {
                Vector3 aim = Vector3Add(Vector3Scale(cr, rx), Vector3Scale(cf, -ry));
                lastAimDir = Vector3Normalize(aim);
            }
            else {
                if (isMoving) {
                    lastAimDir = Vector3Normalize(md);
                }
                else {
                    Vector3 camFwd = Vector3Normalize(Vector3Subtract(cam.target, cam.position));
                    camFwd.y = 0; camFwd = Vector3Normalize(camFwd);
                    lastAimDir = camFwd;
                }
            }
        }
    }

    if (attackTimer > 0) attackTimer -= dt;
    bool attackInput = IsMouseButtonPressed(0) || IsGamepadButtonPressed(0, DataManager::keyConfig.padAttack);
    if (attackInput && attackTimer <= 0 && currentWeapon != NONE) {
        PerformAttack(lastAimDir, enemies, d, fx);
        attackTimer = 0.5f; animTime = 0;
    }

    float cdMultiplier = 1.0f - cooldownReduction;
    if (cdMultiplier < 0.2f) cdMultiplier = 0.2f;

    bool btnDash = IsKeyPressed(DataManager::keyConfig.dash) || IsGamepadButtonPressed(0, DataManager::keyConfig.padDash);
    bool btnSmash = IsKeyPressed(DataManager::keyConfig.smash) || IsGamepadButtonPressed(0, DataManager::keyConfig.padSmash);
    bool btnKongo = IsKeyPressed(DataManager::keyConfig.kongo) || IsGamepadButtonPressed(0, DataManager::keyConfig.padKongo);
    bool btnZoukyou = IsKeyPressed(DataManager::keyConfig.zoukyou) || IsGamepadButtonPressed(0, DataManager::keyConfig.padZoukyou);
    bool btnStealth = IsKeyPressed(DataManager::keyConfig.stealth) || IsGamepadButtonPressed(0, DataManager::keyConfig.padStealth);
    bool btnHeal = IsKeyPressed(DataManager::keyConfig.heal) || IsGamepadButtonPressed(0, DataManager::keyConfig.padHeal);
    bool btnSwap = IsKeyPressed(DataManager::keyConfig.swapWeapon) || IsGamepadButtonPressed(0, DataManager::keyConfig.padSwap);

    if (btnDash && IsSkillUnlocked(SKILL_ACTIVE_DASH) && dashCooldownTimer <= 0) {
        dashTimer = 0.35f; dashCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_DASH) * cdMultiplier; AudioManager::PlaySE(SE_SKILL);
    }
    if (btnSmash && IsSkillUnlocked(SKILL_ACTIVE_SMASH) && smashCooldownTimer <= 0 && attackTimer <= 0) {
        PerformSmash(lastAimDir, enemies, d, fx); smashCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_SMASH) * cdMultiplier; attackTimer = 0.8f; animTime = 0;
    }
    if (btnKongo && IsSkillUnlocked(SKILL_ACTIVE_KONGO) && kongoCooldownTimer <= 0) {
        kongoTimer = 10.0f; kongoCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_KONGO) * cdMultiplier; AudioManager::PlaySE(SE_SKILL); fx.SpawnEffect(position, { 0,1,0 }, FX_HIT, GOLD); RecalculateStats();
    }
    if (btnZoukyou && IsSkillUnlocked(SKILL_ACTIVE_ZOUKYOU) && zoukyouCooldownTimer <= 0) {
        zoukyouTimer = 10.0f; zoukyouCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_ZOUKYOU) * cdMultiplier; AudioManager::PlaySE(SE_SKILL); fx.SpawnEffect(position, { 0,1,0 }, FX_HIT, RED); RecalculateStats();
    }
    if (btnStealth && IsSkillUnlocked(SKILL_ACTIVE_STEALTH) && stealthCooldownTimer <= 0) {
        stealthTimer = 10.0f; stealthCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_STEALTH) * cdMultiplier; isStealth = true; AudioManager::PlaySE(SE_SKILL); fx.SpawnEffect(position, { 0,1,0 }, FX_HIT, BLUE);
    }
    if (btnHeal && IsSkillUnlocked(SKILL_ACTIVE_HEAL) && healCooldownTimer <= 0) {
        hp += (maxHp * 0.3f) + healBonus; if (hp > maxHp) hp = maxHp;
        healCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_HEAL) * cdMultiplier; AudioManager::PlaySE(SE_HEAL); fx.SpawnEffect(position, { 0,1,0 }, FX_HIT, GREEN);
    }

    if (btnSwap) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }

    if (attackTimer > 0) modelRotation = atan2f(lastAimDir.x, lastAimDir.z) * RAD2DEG;

    int targetAnim = 4;
    if (hp <= 0) targetAnim = 3;
    else if (attackTimer > 0) targetAnim = (currentWeapon == SWORD ? 0 : (currentWeapon == AXE ? 1 : (currentWeapon == WAND ? 2 : 0)));
    else if (dashTimer > 0) targetAnim = 6;
    else if (isMoving) targetAnim = 5;

    if (targetAnim != currentAnimIndex) {
        bool isLoopGroup = (targetAnim >= 4);
        bool wasLoopGroup = (currentAnimIndex >= 4);
        if (!(isLoopGroup && wasLoopGroup)) animTime = 0;
        currentAnimIndex = targetAnim;
    }

    float pSpd = 30.0f;
    if (currentAnimIndex == 5) pSpd = 30.0f * (curSpd / 0.18f);
    else if (currentAnimIndex == 6) pSpd = 30.0f * (curSpd / 0.45f);
    animTime += dt * pSpd;
}

void Player::Draw(bool debug) {
    if (DataManager::loadedModels.count("Player") == 0) return;
    GameModel& gm = DataManager::loadedModels["Player"];
    if (gm.animCount <= currentAnimIndex) return;

    ModelAnimation anim = gm.anims[currentAnimIndex];
    int frame = (currentAnimIndex <= 3) ? (int)fminf(animTime, (float)anim.frameCount - 1) : (int)fmodf(animTime, (float)anim.frameCount);

    UpdateModelAnimation(gm.model, anim, frame);
    for (int i = 0; i < gm.model.boneCount; i++) gm.model.bindPose[i].scale = { 1.0f, 1.0f, 1.0f };

    float scale = 0.01f;
    float yOffset = -0.4f;
    Vector3 drawPos = { position.x, position.y + yOffset, position.z };

    gm.model.transform = MatrixMultiply(MatrixRotateX(-90 * DEG2RAD), MatrixRotateY(modelRotation * DEG2RAD));
    DrawModel(gm.model, drawPos, scale, (isStealth ? Fade(BLUE, 0.4f) : WHITE));

    if (currentWeapon != NONE && !isDead) {
        int handIdx = -1;
        for (int i = 0; i < gm.model.boneCount; i++) {
            std::string bName = gm.model.bones[i].name;
            for (auto& c : bName) c = (char)tolower(c);

            if (bName.find("hand_r") != std::string::npos ||
                bName.find("handright") != std::string::npos ||
                bName.find("hand.r") != std::string::npos) {
                handIdx = i;
                break;
            }
        }

        if (handIdx != -1) {
            Matrix boneMat = GetPlayerBoneGlobalMatrix(gm.model, anim, frame, handIdx);

            Matrix playerWorld = MatrixMultiply(
                MatrixMultiply(MatrixScale(scale, scale, scale), gm.model.transform),
                MatrixTranslate(drawPos.x, drawPos.y, drawPos.z)
            );

            Matrix weaponScale = MatrixScale(customWeaponScale, customWeaponScale, customWeaponScale);

            Matrix offsetMatrix = MatrixMultiply(
                MatrixRotateXYZ({ customWeaponOffsetRot.x * DEG2RAD, customWeaponOffsetRot.y * DEG2RAD, customWeaponOffsetRot.z * DEG2RAD }),
                MatrixTranslate(customWeaponOffsetPos.x, customWeaponOffsetPos.y, customWeaponOffsetPos.z)
            );

            Matrix finalTransform = MatrixMultiply(weaponScale, offsetMatrix);
            finalTransform = MatrixMultiply(finalTransform, boneMat);
            finalTransform = MatrixMultiply(finalTransform, playerWorld);

            int equipId = equippedData[activeSlot].id;
            std::string baseKey = (currentWeapon == SWORD) ? "Wpn_Sword" : (currentWeapon == AXE ? "Wpn_Axe" : (currentWeapon == WAND ? "Wpn_Wand" : "Wpn_Spear"));
            std::string finalKey = baseKey;

            std::string customName = equippedData[activeSlot].modelName;
            if (!customName.empty() && DataManager::loadedModels.count(customName) > 0) {
                finalKey = customName;
            }
            else {
                if (equipId >= 400 && equipId < 500) { if (DataManager::loadedModels.count(baseKey + "_Legend")) finalKey = baseKey + "_Legend"; }
                if (DataManager::loadedModels.count("Wpn_" + std::to_string(equipId))) finalKey = "Wpn_" + std::to_string(equipId);
            }

            if (DataManager::loadedModels.count(finalKey)) {
                Model& wm = DataManager::loadedModels[finalKey].model;
                wm.transform = finalTransform;
                DrawModel(wm, { 0,0,0 }, 1.0f, WHITE);
                wm.transform = MatrixIdentity();
            }
            else {
                Model& wm = DataManager::fallbackWeaponModel;
                wm.transform = finalTransform;
                DrawModel(wm, { 0,0,0 }, 1.0f, RED);
                DrawModelWires(wm, { 0,0,0 }, 1.0f, MAROON);
                wm.transform = MatrixIdentity();
            }

            // ★常にテスト用の青い球とXYZ軸の線を描画する処理を復活
            Vector3 handPos = { finalTransform.m12, finalTransform.m13, finalTransform.m14 };
            DrawSphere(handPos, 0.2f, Fade(BLUE, 0.8f));

            Vector3 right = { finalTransform.m0, finalTransform.m1, finalTransform.m2 };
            Vector3 up = { finalTransform.m4, finalTransform.m5, finalTransform.m6 };
            Vector3 forward = { finalTransform.m8, finalTransform.m9, finalTransform.m10 };

            right = Vector3Normalize(right); up = Vector3Normalize(up); forward = Vector3Normalize(forward);

            DrawLine3D(handPos, Vector3Add(handPos, Vector3Scale(right, 2.0f)), RED);   // X
            DrawLine3D(handPos, Vector3Add(handPos, Vector3Scale(up, 2.0f)), GREEN);    // Y
            DrawLine3D(handPos, Vector3Add(handPos, Vector3Scale(forward, 2.0f)), BLUE); // Z
        }
    }
    gm.model.transform = MatrixIdentity();
}