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

// --- コンストラクタ ---
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
    isStealth = false;

    // アニメーション初期化
    animTime = 0.0f;
    currentAnimIndex = 4; // 4: Idle
    prevAnimIndex = 4;
    modelRotation = 0.0f;
    isDead = false;

    // 初期装備の解決（DataManager経由）
    ItemData s1 = DataManager::GetItemConfigCopy(0);
    ItemData s2 = DataManager::GetItemConfigCopy(100);
    if (s1.id != -1) { equippedData[0] = s1; equippedWeapons[0] = (WeaponType)s1.weaponSubtype; }
    if (s2.id != -1) { equippedData[1] = s2; equippedWeapons[1] = (WeaponType)s2.weaponSubtype; }
    currentWeapon = equippedWeapons[0];

    InitSkillTree();
    RecalculateStats();
}

// --- スタティックメソッド (アイテム情報の表示・計算) ---
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

// --- ステータス・スキル管理 ---
void Player::RecalculateStats() {
    float bHp = 100.0f + (level - 1) * 20.0f;
    float bAtk = 12.0f + (level - 1) * 2.0f;
    float bDef = 5.0f + (level - 1) * 1.5f;
    float bSpd = 0.18f;
    for (const auto& node : skillTree) if (node.unlocked) { bAtk += node.atkAdd; bDef += node.defAdd; bHp += node.hpAdd; }
    for (int i = 0; i < 5; i++) if (equippedArmor[i].id != -1) {
        Modifier m = DataManager::GetModifier(equippedArmor[i].modifierId);
        bHp += equippedArmor[i].hpBonus + m.hp; bDef += equippedArmor[i].defBonus + m.def;
        bAtk += equippedArmor[i].atkBonus + m.atk; bSpd += equippedArmor[i].speedBonus + m.spd;
    }
    maxHp = bHp; attackPower = bAtk; defense = bDef; baseSpeed = bSpd;
    if (hp > maxHp) hp = maxHp;
}

void Player::InitSkillTree() {
    auto T = [](const char* k) { return DataManager::uiStrings.count(k) ? DataManager::uiStrings[k] : k; };
    skillTree.clear();
    skillTree.push_back({ 0, "START", {640, 650}, {}, true, 0 });
    skillTree.push_back({ 1, "HP I",  {500, 550}, {0}, false, 1, 0, 0, 30, SKILL_PASSIVE });
    skillTree.push_back({ 3, T("STEALTH"), {300, 400}, {1}, false, 3, 0, 0, 0, SKILL_ACTIVE_STEALTH, 15.0f });
    skillTree.push_back({ 6, T("SMASH"),   {640, 400}, {5}, false, 2, 0, 0, 0, SKILL_ACTIVE_SMASH, 5.0f });
    skillTree.push_back({ 10, T("DASH"), {880, 480}, {9}, false, 2, 0, 0, 0, SKILL_ACTIVE_DASH, 3.0f });
}

bool Player::IsSkillAvailable(int id) {
    for (auto& n : skillTree) if (n.id == id) { if (n.unlocked) return false; if (n.reqIds.empty()) return true; for (int r : n.reqIds) for (auto& rn : skillTree) if (rn.id == r && rn.unlocked) return true; }
    return false;
}

void Player::UnlockSkill(int id) {
    if (IsSkillAvailable(id)) { for (auto& n : skillTree) if (n.id == id && skillPoints >= n.cost) { skillPoints -= n.cost; n.unlocked = true; AudioManager::PlaySE(SE_SKILL); RecalculateStats(); return; } }
}

bool Player::IsSkillUnlocked(SkillType t) { for (auto& n : skillTree) if (n.type == t && n.unlocked) return true; return false; }
float Player::GetSkillCooldown(SkillType t) { return (t == SKILL_ACTIVE_DASH) ? dashCooldownTimer : (t == SKILL_ACTIVE_SMASH ? smashCooldownTimer : stealthCooldownTimer); }
float Player::GetSkillMaxCooldown(SkillType t) { for (auto& n : skillTree) if (n.type == t) return n.maxCooldown; return 1.0f; }

// --- 経験値・インベントリ ---
void Player::AddExp(int a, EffectManager& fx) { exp += a; while (exp >= expToNext) LevelUp(fx); }
void Player::LevelUp(EffectManager& fx) {
    level++; exp -= expToNext; expToNext = (int)(expToNext * 1.5f); skillPoints += 3;
    fx.SpawnDamageText(position, 999); AudioManager::PlaySE(SE_LEVELUP);
    RecalculateStats(); hp = maxHp;
    UI::AddSystemLog("LEVEL UP!", YELLOW);
}

bool Player::AddToInventory(ItemData item) {
    if (item.type == "EQUIP" || item.type == "ARMOR") { if (inventoryEquip.size() >= MAX_EQUIP_INV) return false; inventoryEquip.push_back(item); }
    else { for (auto& i : inventoryItems) if (i.id == item.id && i.count < MAX_ITEM_STACK) { i.count++; return true; } if (inventoryItems.size() >= MAX_ITEM_TYPES) return false; inventoryItems.push_back(item); }
    return true;
}

void Player::UseItem(int idx) { if (idx >= 0 && idx < (int)inventoryItems.size() && inventoryItems[idx].type == "CONSUMABLE") { hp = fminf(maxHp, hp + inventoryItems[idx].heal); if (--inventoryItems[idx].count <= 0) inventoryItems.erase(inventoryItems.begin() + idx); AudioManager::PlaySE(SE_HEAL); } }

// --- 装備操作 ---
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

// --- クエスト ---
void Player::UpdateHuntQuest(int eId) { for (auto& q : activeQuests) if (!q.isCompleted) { QuestData d = DataManager::GetQuestData(q.questId); if (d.type == QUEST_HUNT && d.targetId == eId) if (++q.currentCount >= d.targetCount) q.isCompleted = true; } }
bool Player::CheckGatherQuest(int itemId, int count) { int sum = 0; for (auto& i : inventoryItems) if (i.id == itemId) sum += i.count; return sum >= count; }
void Player::CompleteQuest(int qId) { for (auto it = activeQuests.begin(); it != activeQuests.end(); ++it) if (it->questId == qId) { QuestData d = DataManager::GetQuestData(qId); gold += d.rewardGold; if (d.rewardItemId != -1) { ItemData r = DataManager::GetItemConfigCopy(d.rewardItemId); r.count = d.rewardItemCount; AddToInventory(r); } clearedQuests.push_back(qId); activeQuests.erase(it); AudioManager::PlaySE(SE_REFORGE); return; } }

// --- 戦闘アクション ---
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
            isStealth = false;
        }
    }
}

void Player::PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
    AudioManager::PlaySE(SE_SKILL);
    fx.SpawnEffect(Vector3Add(position, { 0, 0.8f, 0 }), ad, FX_SMASH, RED);
    for (auto& e : enemies) if (Vector3Distance(e.position, position) < 5.0f) {
        int dmg = (int)(attackPower * 2.5f); e.hp -= dmg; e.ApplyKnockback(ad, 3.0f, d); fx.SpawnDamageText(e.position, dmg);
        isStealth = false;
    }
}

// --- メイン更新ループ ---
void Player::Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop) {
    if (stop) return;
    float dt = GetFrameTime();

    if (dashCooldownTimer > 0) dashCooldownTimer -= dt;
    if (smashCooldownTimer > 0) smashCooldownTimer -= dt;
    if (stealthCooldownTimer > 0) stealthCooldownTimer -= dt;
    if (dashTimer > 0) dashTimer -= dt;
    if (stealthTimer > 0) stealthTimer -= dt; else isStealth = false;

    // 移動制御
    Vector3 cf = Vector3Normalize(Vector3Subtract(cam.target, cam.position)); cf.y = 0; cf = Vector3Normalize(cf);
    Vector3 cr = { -cf.z, 0, cf.x }, md = { 0,0,0 };
    if (IsKeyDown(KEY_W)) md = Vector3Add(md, cf); if (IsKeyDown(KEY_S)) md = Vector3Subtract(md, cf);
    if (IsKeyDown(KEY_A)) md = Vector3Subtract(md, cr); if (IsKeyDown(KEY_D)) md = Vector3Add(md, cr);

    bool isMoving = (Vector3Length(md) > 0.1f);
    float curSpd = (dashTimer > 0) ? baseSpeed * 2.8f : baseSpeed;

    if (isMoving) {
        md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, curSpd);
        if (!d.CheckCollisionRadius(Vector3Add(position, { v.x,0,0 }), radius)) position.x += v.x;
        if (!d.CheckCollisionRadius(Vector3Add(position, { 0,0,v.z }), radius)) position.z += v.z;
        modelRotation = atan2f(md.x, md.z) * RAD2DEG;
    }

    // 視線と攻撃
    Ray ray = GetMouseRay(GetMousePosition(), cam);
    if (ray.direction.y != 0) {
        float t = (position.y - ray.position.y) / ray.direction.y;
        Vector3 tp = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        lastAimDir = Vector3Normalize(Vector3Subtract(tp, position)); lastAimDir.y = 0;
    }
    if (attackTimer > 0) attackTimer -= dt;
    if (IsMouseButtonPressed(0) && attackTimer <= 0 && currentWeapon != NONE) { PerformAttack(lastAimDir, enemies, d, fx); attackTimer = 0.5f; animTime = 0; }
    if (IsKeyPressed(KEY_ONE) && IsSkillUnlocked(SKILL_ACTIVE_DASH) && dashCooldownTimer <= 0) { dashTimer = 0.35f; dashCooldownTimer = 3.0f; AudioManager::PlaySE(SE_SKILL); }
    if (IsKeyPressed(KEY_TWO) && IsSkillUnlocked(SKILL_ACTIVE_SMASH) && smashCooldownTimer <= 0 && attackTimer <= 0) { PerformSmash(lastAimDir, enemies, d, fx); smashCooldownTimer = 5.0f; attackTimer = 0.8f; animTime = 0; }
    if (IsKeyPressed(KEY_Q)) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }

    if (attackTimer > 0) modelRotation = atan2f(lastAimDir.x, lastAimDir.z) * RAD2DEG;

    // --- アニメーション状態の決定 (0:Atk, 3:Die, 4:Idle, 5:Run, 6:Sprint) ---
    int targetAnim = 4;
    if (hp <= 0) targetAnim = 3;
    else if (attackTimer > 0) targetAnim = (currentWeapon == SWORD ? 0 : (currentWeapon == AXE ? 1 : 2));
    else if (dashTimer > 0) targetAnim = 6;
    else if (isMoving) targetAnim = 5;

    // ポーズのワープ防止: 移動グループ(4,5,6)間では時間をリセットしない
    if (targetAnim != currentAnimIndex) {
        bool isLoopGroup = (targetAnim >= 4);
        bool wasLoopGroup = (currentAnimIndex >= 4);
        if (!(isLoopGroup && wasLoopGroup)) animTime = 0;
        currentAnimIndex = targetAnim;
    }

    // 再生速度同期
    float pSpd = 30.0f;
    if (currentAnimIndex == 5) pSpd = 30.0f * (curSpd / 0.18f);      // Run速度同期
    else if (currentAnimIndex == 6) pSpd = 30.0f * (curSpd / 0.45f); // Sprint速度同期
    animTime += dt * pSpd;
}

// --- 描画処理 ---
void Player::Draw(bool debug) {
    if (DataManager::loadedModels.count("Player") == 0) return;
    GameModel& gm = DataManager::loadedModels["Player"];
    if (gm.animCount <= currentAnimIndex) return;

    ModelAnimation anim = gm.anims[currentAnimIndex];
    int frame = (currentAnimIndex <= 3) ? (int)fminf(animTime, (float)anim.frameCount - 1) : (int)fmodf(animTime, (float)anim.frameCount);

    // ポーズの更新
    UpdateModelAnimation(gm.model, anim, frame);
    // IQM正規化 (スケール爆発防止)
    for (int i = 0; i < gm.model.boneCount; i++) gm.model.bindPose[i].scale = { 1.0f, 1.0f, 1.0f };

    // ★修正: モデルの表示位置を少し下げる（原点より高い問題を解消）
    float scale = 0.01f;
    float yOffset = -0.15f; // 必要に応じてこの値を調整してください
    Vector3 drawPos = { position.x, position.y + yOffset, position.z };

    gm.model.transform = MatrixMultiply(MatrixRotateX(-90 * DEG2RAD), MatrixRotateY(modelRotation * DEG2RAD));
    DrawModel(gm.model, drawPos, scale, (isStealth ? Fade(BLUE, 0.4f) : WHITE));

    // 武器アタッチメント
    if (currentWeapon != NONE && !isDead) {
        int handIdx = -1;
        for (int i = 0; i < gm.model.boneCount; i++) {
            std::string bName = gm.model.bones[i].name;
            for (auto& c : bName) c = (char)tolower(c);
            if (bName.find("weapon_r") != std::string::npos || bName.find("hand_r") != std::string::npos) { handIdx = i; break; }
        }
        if (handIdx != -1) {
            Transform b = gm.model.bindPose[handIdx];
            Matrix boneMat = MatrixMultiply(QuaternionToMatrix(b.rotation), MatrixTranslate(b.translation.x, b.translation.y, b.translation.z));
            // 武器もプレイヤーの下げた位置(drawPos)に合わせる
            Matrix playerWorld = MatrixMultiply(MatrixMultiply(MatrixScale(scale, scale, scale), gm.model.transform), MatrixTranslate(drawPos.x, drawPos.y, drawPos.z));

            int equipId = equippedData[activeSlot].id;
            std::string baseKey = (currentWeapon == SWORD) ? "Wpn_Sword" : (currentWeapon == AXE ? "Wpn_Axe" : "Wpn_Spear");
            std::string finalKey = baseKey;
            if (equipId >= 400 && equipId < 500) { if (DataManager::loadedModels.count(baseKey + "_Legend")) finalKey = baseKey + "_Legend"; }
            if (DataManager::loadedModels.count("Wpn_" + std::to_string(equipId))) finalKey = "Wpn_" + std::to_string(equipId);

            if (DataManager::loadedModels.count(finalKey)) {
                Model& wm = DataManager::loadedModels[finalKey].model;
                wm.transform = MatrixMultiply(boneMat, playerWorld);
                DrawModel(wm, { 0,0,0 }, 1.0f, WHITE);
                wm.transform = MatrixIdentity();
            }
        }
    }
    gm.model.transform = MatrixIdentity();
}