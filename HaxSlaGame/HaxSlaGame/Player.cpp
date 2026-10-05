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

// 変数の実体定義 (F1メニューで調整したデフォルト値)
Vector3 Player::customWeaponOffsetPos = { -10.0f, 0.0f, 0.0f };
Vector3 Player::customWeaponOffsetRot = { 90.0f, 180.0f, 0.0f };
float Player::customWeaponScale = 100.0f;

static std::string T(const std::string& key, const std::string& def) {
	if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
	return def;
}

// 【重要】アニメーションしている手の位置(ボーン)のグローバル行列を取得する
// これにより、手が動くのに合わせて武器も全く同じように動かすことができる
static Matrix GetPlayerBoneGlobalMatrix(Model model, ModelAnimation anim, int frame, int boneIndex) {
	if (boneIndex < 0 || boneIndex >= model.boneCount) return MatrixIdentity();
	if (anim.frameCount <= 0 || anim.framePoses == nullptr) return MatrixIdentity();

	if (frame < 0) frame = 0;
	if (frame >= anim.frameCount) frame = anim.frameCount - 1;

	// RaylibのIQMアニメーション(framePoses)には既にルートからの絶対座標が格納されているため、
	// 親ボーンを辿って乗算する必要はない。単一ボーンを取り出すだけでよい。
	Transform b = anim.framePoses[frame][boneIndex];

	// ボーン自体のスケール(b.scale)を掛けてしまうと、武器が巨大化したり歪んだりするため除外する。
	// 回転と移動だけを適用した純粋な手の動きの行列を生成。
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

	isChargingSmash = false;
	smashChargeTimer = 0.0f;
	smashChargeMax = 0.6f;
	smashChargeDir = { 1, 0, 0 };


	animTime = 0.0f;
	currentAnimIndex = 4;
	prevAnimIndex = 4;
	modelRotation = 0.0f;
	isDead = false;

	// 初期装備として「木の剣(id=0)」だけを持たせる
	ItemData s1 = DataManager::GetItemConfigCopy(0);
	if (s1.id != -1) { equippedData[0] = s1; equippedWeapons[0] = (WeaponType)s1.weaponSubtype; }
	currentWeapon = equippedWeapons[0];

	InitSkillTree();
	RecalculateStats();
}


float Player::GetItemTotalAtkBonus(const ItemData& item) {
	if (item.id == -1) return 0.0f;
	return item.atkBonus + DataManager::GetModifier(item.modifierId).atk;
}

std::string Player::GetElementName(int elem) {
	switch (elem) {
	case ELEM_HELLFIRE: return T("ELEM_HELLFIRE", "Hellfire ");
	case ELEM_ROT:      return T("ELEM_ROT", "Rot ");
	case ELEM_SOUL:     return T("ELEM_SOUL", "Soul ");
	case ELEM_ABYSS:    return T("ELEM_ABYSS", "Abyss ");
	default:            return "";
	}
}

Color Player::GetElementColor(int elem) {
	switch (elem) {
	case ELEM_HELLFIRE: return Color{ 255, 60, 40, 255 };   // 深紅
	case ELEM_ROT:      return Color{ 80, 220, 60, 255 };   // 毒緑
	case ELEM_SOUL:     return Color{ 100, 200, 255, 255 }; // 幽青
	case ELEM_ABYSS:    return Color{ 180, 50, 230, 255 };  // 深紫
	default:            return WHITE;
	}
}

// アイテムのフルネーム（例：「伝説の 業火の 機神の滅斧」）
std::string Player::GetFullItemName(const ItemData& item) {
	if (item.id == -1) return "EMPTY";
	Modifier mod = DataManager::GetModifier(item.modifierId);
	std::string elemStr = GetElementName(item.element);

	std::string fullName = "";
	if (!mod.name.empty()) fullName += mod.name + " ";
	if (!elemStr.empty()) fullName += elemStr + " ";
	fullName += item.name;
	return fullName;
}

float Player::GetElementMultiplier(int atkElem, int defElem) {
	if (atkElem == ELEM_NONE || defElem == ELEM_NONE) return 1.0f;

	// 深淵（Abyss）：全属性に1.25倍
	if (atkElem == ELEM_ABYSS) return 1.25f;

	// ★ 同属性耐性：同じ属性で攻撃すると耐性によりダメージ軽減（0.6倍）
	if (atkElem == defElem) return 0.60f;

	// 3すくみ相性
	if (atkElem == ELEM_HELLFIRE && defElem == ELEM_ROT)  return 1.40f; // 業火 → 腐蝕（特効 1.4倍）
	if (atkElem == ELEM_ROT && defElem == ELEM_SOUL) return 1.40f; // 腐蝕 → 霊怨（特効 1.4倍）
	if (atkElem == ELEM_SOUL && defElem == ELEM_HELLFIRE) return 1.40f; // 霊怨 → 業火（特効 1.4倍）

	// 不利属性
	if (atkElem == ELEM_HELLFIRE && defElem == ELEM_SOUL) return 0.75f;
	if (atkElem == ELEM_ROT && defElem == ELEM_HELLFIRE) return 0.75f;
	if (atkElem == ELEM_SOUL && defElem == ELEM_ROT)  return 0.75f;

	return 1.0f;
}

// 装備中の防具5部位から、特定属性の耐性合計を算出（1部位につき +15%）
float Player::GetPlayerElementResistance(int elem) {
	if (elem == ELEM_NONE) return 0.0f;
	float totalRes = 0.0f;
	for (int i = 0; i < 5; i++) {
		if (equippedArmor[i].id != -1 && equippedArmor[i].element == elem) {
			totalRes += 0.15f;
		}
	}
	if (totalRes > 0.60f) totalRes = 0.60f; // 最大60%上限
	return totalRes;
}

int Player::CompareWithEquipped(const ItemData& item, const Player& p) {
	if (item.id == -1) return 0;

	// --- 武器の比較（合計攻撃力で判定） ---
	if (item.type == "EQUIP") {
		const ItemData& cur = p.equippedData[p.activeSlot];
		float curAtk = 0.0f;
		if (cur.id != -1) {
			curAtk = cur.atkBonus + DataManager::GetModifier(cur.modifierId).atk;
		}
		else {
			return 1; // 現在何も装備していなければ必ず強い
		}

		float itemAtk = item.atkBonus + DataManager::GetModifier(item.modifierId).atk;
		if (itemAtk > curAtk + 0.01f) return 1;   // 強い (▲)
		if (itemAtk < curAtk - 0.01f) return -1;  // 弱い (▼)
		return 0;
	}

	// --- 防具の比較（合計防御力で判定） ---
	if (item.type == "ARMOR") {
		int sub = item.weaponSubtype;
		if (sub < 0 || sub >= 5) return 0;

		const ItemData& cur = p.equippedArmor[sub];
		float curDef = 0.0f;
		if (cur.id != -1) {
			curDef = cur.defBonus + DataManager::GetModifier(cur.modifierId).def;
		}
		else {
			return 1; // スロットが空なら必ず強い
		}

		float itemDef = item.defBonus + DataManager::GetModifier(item.modifierId).def;
		if (itemDef > curDef + 0.01f) return 1;   // 強い (▲)
		if (itemDef < curDef - 0.01f) return -1;  // 弱い (▼)
		return 0;
	}

	return 0;
}


// アイテムのID帯とエンチャントの有無によって、レアリティ色を自動決定する
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
	if (item.modifierId != 0 && tier < 5) tier++; // エンチャント付きはレアリティが1段階上がる
	switch (tier) {
	case 1: return WHITE; case 2: return GREEN; case 3: return SKYBLUE;
	case 4: return PURPLE; case 5: return GOLD; default: return WHITE;
	}
}

int Player::GetItemTier(const ItemData& item) {
	if (item.id == -1) return 0;
	if (item.type == "MATERIAL" || item.type == "CONSUMABLE") return 1;

	int tier = 1;
	if (item.id >= 100 && item.id < 200) tier = 1;
	else if (item.id >= 200 && item.id < 300) tier = 2;
	else if (item.id >= 300 && item.id < 400) tier = 3;
	else if (item.id >= 400 && item.id < 500) tier = 4;
	else if (item.id >= 500) tier = 5;

	if (item.modifierId != 0 && tier < 5) tier++; // エンチャント付きは+1
	return tier;
}

// 装備とスキルの効果を再計算して、最終的なHPや攻撃力を割り出す
void Player::RecalculateStats() {
	float bHp = 100.0f + (level - 1) * 20.0f;
	float bAtk = 12.0f + (level - 1) * 2.0f;
	float bDef = 5.0f + (level - 1) * 1.5f;
	float bSpd = 0.18f;
	cooldownReduction = 0.0f;
	healBonus = 0.0f;

	for (const auto& node : skillTree) if (node.unlocked) {
		bAtk += node.atkAdd; bDef += node.defAdd; bHp += node.hpAdd;
		cooldownReduction += node.cdRedAdd; healBonus += node.healAdd;
	}

	if (zoukyouTimer > 0) bAtk *= 1.5f; // アクティブスキル「増強」中は攻撃力1.5倍
	if (kongoTimer > 0) bDef += 20.0f;  // アクティブスキル「金剛」中は防御力固定値アップ

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

	// =========================================================================
	// 【中心】起点 (ID: 0)
	// =========================================================================
	skillTree.push_back({ 0, T("SKILL_NAME_START", "START"), {600, 400}, {}, true, 0, 0, 0, 0, 0, 0, T("SKILL_DESC_START", "Skill Tree Start"), SKILL_PASSIVE });

	// =========================================================================
	// 【12時方向（真上）】 攻撃（ATK）系統
	// =========================================================================
	skillTree.push_back({ 1, T("SKILL_NAME_ATK1", "ATK I"),   {600, 290}, {0}, false, 1, 3.0f, 0, 0, 0, 0, T("SKILL_DESC_ATK1", "ATK +3"), SKILL_PASSIVE });
	skillTree.push_back({ 2, T("SKILL_NAME_ATK2", "ATK II"),  {600, 190}, {1}, false, 2, 5.0f, 0, 0, 0, 0, T("SKILL_DESC_ATK2", "ATK +5"), SKILL_PASSIVE });
	skillTree.push_back({ 3, T("SMASH", "SMASH"),             {600, 90},  {2}, false, 3, 0,    0, 0, 0, 0, T("SKILL_DESC_SMASH", "Active: Deal heavy damage & knockback"), SKILL_ACTIVE_SMASH, 8.0f });

	// =========================================================================
	// 【2時方向（右上）】 防御（DEF）系統
	// =========================================================================
	skillTree.push_back({ 4, T("SKILL_NAME_DEF1", "DEF I"),   {695, 345}, {0}, false, 1, 0, 2.0f, 0, 0, 0, T("SKILL_DESC_DEF1", "DEF +2"), SKILL_PASSIVE });
	skillTree.push_back({ 5, T("SKILL_NAME_DEF2", "DEF II"),  {782, 295}, {4}, false, 2, 0, 3.0f, 0, 0, 0, T("SKILL_DESC_DEF2", "DEF +3"), SKILL_PASSIVE });
	skillTree.push_back({ 6, T("KONGO", "KONGO"),             {868, 245}, {5}, false, 3, 0, 0,    0, 0, 0, T("SKILL_DESC_KONGO", "Active: Boost DEF temporarily"), SKILL_ACTIVE_KONGO, 15.0f });

	// =========================================================================
	// 【4時方向（右下）】 体力・バフ（HP / ZOUKYOU）系統
	// =========================================================================
	skillTree.push_back({ 7, T("SKILL_NAME_HP1", "HP I"),     {695, 455}, {0}, false, 1, 0, 0, 20.0f, 0, 0, T("SKILL_DESC_HP1", "HP +20"), SKILL_PASSIVE });
	skillTree.push_back({ 8, T("SKILL_NAME_HP2", "HP II"),    {782, 505}, {7}, false, 2, 0, 0, 30.0f, 0, 0, T("SKILL_DESC_HP2", "HP +30"), SKILL_PASSIVE });
	skillTree.push_back({ 9, T("ZOUKYOU", "ZOUKYOU"),         {868, 555}, {8}, false, 3, 0, 0, 0,     0, 0, T("SKILL_DESC_ZOUKYOU", "Active: Boost ATK temporarily"), SKILL_ACTIVE_ZOUKYOU, 20.0f });

	// =========================================================================
	// ★【6時方向（真下）】 回避・機動（DASH）系統
	// =========================================================================
	// 起点から真下にまっすぐ伸びる独立したアクティブノード
	skillTree.push_back({ 16, T("DASH", "DASH"),              {600, 520}, {0}, false, 1, 0, 0, 0,     0, 0, T("SKILL_DESC_DASH", "Active: Quick dodge"), SKILL_ACTIVE_DASH, 3.0f });

	// =========================================================================
	// 【8時方向（左下）】 クールダウン短縮・潜行（CD / STEALTH）系統
	// =========================================================================
	skillTree.push_back({ 10, T("SKILL_NAME_CD1", "CD I"),    {505, 455}, {0},  false, 1, 0, 0, 0, 0.05f, 0, T("SKILL_DESC_CD1", "Cooldown -5%"), SKILL_PASSIVE });
	skillTree.push_back({ 11, T("SKILL_NAME_CD2", "CD II"),   {418, 505}, {10}, false, 2, 0, 0, 0, 0.10f, 0, T("SKILL_DESC_CD2", "Cooldown -10%"), SKILL_PASSIVE });
	skillTree.push_back({ 12, T("STEALTH", "STEALTH"),        {332, 555}, {11}, false, 3, 0, 0, 0, 0,     0, T("SKILL_DESC_STEALTH", "Active: Become undetectable"), SKILL_ACTIVE_STEALTH, 15.0f });

	// =========================================================================
	// 【10時方向（左上）】 回復（HEAL）系統
	// =========================================================================
	skillTree.push_back({ 13, T("SKILL_NAME_HEAL1", "HEAL I"),{505, 345}, {0},  false, 1, 0, 0, 0, 0, 10.0f, T("SKILL_DESC_HEAL1", "Healing +10"), SKILL_PASSIVE });
	skillTree.push_back({ 14, T("SKILL_NAME_HEAL2", "HEAL II"),{418, 295},{13}, false, 2, 0, 0, 0, 0, 20.0f, T("SKILL_DESC_HEAL2", "Healing +20"), SKILL_PASSIVE });
	skillTree.push_back({ 15, T("HEAL", "HEAL"),              {332, 245},{14}, false, 3, 0, 0, 0, 0, 0,     T("SKILL_DESC_HEAL", "Active: Restore HP instantly"), SKILL_ACTIVE_HEAL, 25.0f });
}

// そのスキルの手前（前提）スキルがアンロックされているかチェックする
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
	fx.SpawnDamageText(position, 999); AudioManager::PlaySE(SE_LEVELUP); // ダメージテキスト999はLEVELUP専用フラグ
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

// 通常攻撃の実行処理
void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
	AudioManager::PlaySE(SE_ATTACK);
	Vector3 origin = Vector3Add(position, { 0, 0.8f, 0 });

	if (currentWeapon == WAND) {
		fx.SpawnProjectile(origin, ad, 15.0f, 1, true);
	}
	else {
		EffectType type = FX_SLASH;
		Color fxCol = SKYBLUE;
		float atkRange = 2.7f; // 剣の射程

		if (currentWeapon == SPEAR) {
			type = FX_THRUST;
			fxCol = SKYBLUE;
			atkRange = 5.5f;   // 槍の射程
		}
		else if (currentWeapon == AXE) {
			type = FX_SMASH;
			fxCol = ORANGE;
			atkRange = 3.2f;   // 斧の射程
		}

		// エフェクトに射程(atkRange)を渡して見た目の長さを合わせる
		int weaponElem = equippedData[activeSlot].element;
		if (weaponElem != ELEM_NONE) {
			fxCol = GetElementColor(weaponElem);
		}

		fx.SpawnEffect(origin, ad, type, fxCol, atkRange);

		bool hitAny = false;
		for (auto& e : enemies) {
			float dist = Vector3Distance(e.position, position);
			if (dist <= atkRange + e.radius) {
				if (!d.HasLineOfSight(position, e.position)) continue;

				Vector3 toEnemy = Vector3Normalize(Vector3Subtract(e.position, position));
				float dot = Vector3DotProduct(ad, toEnemy);
				float requiredDot = (currentWeapon == SPEAR) ? 0.6f : 0.0f;

				if (dot >= requiredDot) {
					bool isCrit = (GetRandomValue(1, 100) <= 5);
					float baseAtk = attackPower + GetItemTotalAtkBonus(equippedData[activeSlot]);

					// ★ 属性相性倍率の適用（1.4倍 または 0.75倍）
					float elemMulti = GetElementMultiplier(weaponElem, e.data.element);
					int dmg = (int)(baseAtk * elemMulti) + GetRandomValue(0, 3);

					if (isCrit) dmg = (int)((float)dmg * 1.75f);

					e.hp -= dmg;
					e.ApplyKnockback(ad, isCrit ? 1.8f : 1.0f, d);

					// 特効時（1.4倍）は属性色でポップアップ
					Color hitCol = (weaponElem != ELEM_NONE) ? GetElementColor(weaponElem) : (isCrit ? GOLD : RED);
					fx.SpawnDamageText(e.position, dmg, isCrit);
					fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, hitCol);

					// 特効時の専用ログ
					if (elemMulti > 1.1f) {
						UI::AddSystemLog("★ WEAKNESS EXPLOITED! (1.4x) ★", hitCol);
					}
					else if (isCrit) {
						UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_CRIT_DEALT"].c_str(), e.data.name.c_str(), dmg), GOLD);
						fx.TriggerHitStop(0.08f);
						fx.ShakeScreen(0.18f, 0.35f);
					}

					e.hudTimer = 5.0f;
					isStealth = false;
					hitAny = true;
				}
			}
		}

		// ★ 命中時：武器ごとに異なる長さのヒットストップと画面揺れを発動
		if (hitAny) {
			if (currentWeapon == AXE) {
				fx.TriggerHitStop(0.08f);       // 斧：重いヒットストップ
				fx.ShakeScreen(0.12f, 0.25f);  // 斧：ガツンと画面揺れ
			}
			else if (currentWeapon == SPEAR) {
				fx.TriggerHitStop(0.05f);       // 槍：ズスッと貫くヒットストップ
			}
			else if (currentWeapon == SWORD) {
				fx.TriggerHitStop(0.04f);       // 剣：軽快で鋭いヒットストップ
			}
		}
	}
}

// 強撃(SMASH)スキルの実行処理
void Player::PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
	AudioManager::PlaySE(SE_SKILL);
	fx.SpawnEffect(Vector3Add(position, { 0, 0.8f, 0 }), ad, FX_SMASH, RED);
	fx.ShakeScreen(0.25f, 0.5f);
	fx.TriggerHitStop(0.10f);
	for (auto& e : enemies) {
		if (Vector3Distance(e.position, position) < 4.5f) {
			if (!d.HasLineOfSight(position, e.position)) continue;

			// ★ 強撃スキルは25%の確率でクリティカル！
			bool isCrit = (GetRandomValue(1, 100) <= 25);
			int dmg = (int)(attackPower * 2.5f);
			if (isCrit) dmg = (int)(dmg * 1.6f);

			e.hp -= dmg;
			e.ApplyKnockback(ad, 3.5f, d);
			fx.SpawnDamageText(e.position, dmg, isCrit);
			fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, isCrit ? GOLD : RED);

			if (isCrit) {
				UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_CRIT_DEALT"].c_str(), e.data.name.c_str(), dmg), GOLD);
				fx.ShakeScreen(0.35f, 0.7f);   // 特大シェイク
				fx.TriggerHitStop(0.14f);       // 特大ヒットストップ！
			}

			e.hudTimer = 5.0f;
			isStealth = false;
		}
	}
}

// 毎フレームのプレイヤー移動、スキル処理、アニメーション遷移を管理する
// =============================================================================
// 毎フレームのプレイヤー移動、スキル処理、アニメーション遷移を管理する
// =============================================================================
void Player::Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop) {
	if (stop) return;
	float dt = GetFrameTime();

	// --- 各種クールダウンタイマー更新 ---
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

	// ★ 強撃(SMASH)のタメ（チャージ）カウントダウン処理
	if (isChargingSmash) {
		smashChargeTimer -= dt;
		smashChargeDir = lastAimDir; // タメ中もマウス/スティックのエイム方向に狙いを定める

		if (smashChargeTimer <= 0.0f) {
			// タメ完了！ここで特大強撃を炸裂させる！
			isChargingSmash = false;
			PerformSmash(smashChargeDir, enemies, d, fx);

			float cdMultiplier = 1.0f - cooldownReduction;
			if (cdMultiplier < 0.2f) cdMultiplier = 0.2f;
			smashCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_SMASH) * cdMultiplier;

			attackTimer = 0.5f;
			animTime = 0;
		}
	}

	Vector3 cf = Vector3Normalize(Vector3Subtract(cam.target, cam.position)); cf.y = 0; cf = Vector3Normalize(cf);
	Vector3 cr = { -cf.z, 0, cf.x }, md = { 0,0,0 };

	bool isMoving = false;
	float curSpd = baseSpeed;

	// ★ 攻撃中、または強撃タメ中(isChargingSmash)は移動を停止して構える
	if (attackTimer <= 0 && !isChargingSmash) {
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

		isMoving = (Vector3Length(md) > 0.1f);
		curSpd = (dashTimer > 0) ? baseSpeed * 2.8f : baseSpeed;

		if (isMoving) {
			md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, curSpd);
			if (!d.CheckCollisionRadius(Vector3Add(position, { v.x,0,0 }), radius)) position.x += v.x;
			if (!d.CheckCollisionRadius(Vector3Add(position, { 0,0,v.z }), radius)) position.z += v.z;
			modelRotation = atan2f(md.x, md.z) * RAD2DEG;
		}
	}

	// --- エイム（照準）の入力処理 ---
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

	// --- 通常攻撃入力 ---
	if (attackTimer > 0) attackTimer -= dt;
	bool attackInput = IsMouseButtonPressed(0) || IsGamepadButtonPressed(0, DataManager::keyConfig.padAttack);
	if (attackInput && attackTimer <= 0 && !isChargingSmash && currentWeapon != NONE) {
		PerformAttack(lastAimDir, enemies, d, fx);
		attackTimer = 0.5f; animTime = 0;
	}

	float cdMultiplier = 1.0f - cooldownReduction;
	if (cdMultiplier < 0.2f) cdMultiplier = 0.2f;

	// --- スキル入力 ---
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

	// ★ 強撃(SMASH)ボタン入力：即時発動せず、0.6秒のタメ（チャージ）を開始
	if (btnSmash && IsSkillUnlocked(SKILL_ACTIVE_SMASH) && smashCooldownTimer <= 0 && attackTimer <= 0 && !isChargingSmash) {
		isChargingSmash = true;
		smashChargeMax = 0.6f;
		smashChargeTimer = smashChargeMax;
		smashChargeDir = lastAimDir;
		attackTimer = 0.7f; // タメ中の通常攻撃を防止
		AudioManager::PlaySE(SE_CLICK);
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

	if (btnSwap && !isChargingSmash) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }

	// 攻撃中またはタメ中は、照準の方向を向く
	if (attackTimer > 0 || isChargingSmash) modelRotation = atan2f(lastAimDir.x, lastAimDir.z) * RAD2DEG;

	// --- アニメーション決定 ---
	int targetAnim = 4;
	if (hp <= 0) targetAnim = 3;
	else if (isChargingSmash) targetAnim = 4; // タメ中は武器を構える待機ポーズ
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

// =============================================================================
// プレイヤー本体と、手に持った武器を描画する
// =============================================================================
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

	// --- 武器のアタッチメント(持たせる)処理 ---
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
		}
	}
	gm.model.transform = MatrixIdentity();

	// =========================================================================
	// ★ 強撃スキルのAoE予告範囲表示（タメ中に地面に表示）
	// =========================================================================
	if (isChargingSmash) {
		float progress = 1.0f - (smashChargeTimer / smashChargeMax);
		if (progress < 0.0f) progress = 0.0f;
		if (progress > 1.0f) progress = 1.0f;

		float aoeRadius = 3.8f;
		Vector3 aoeCenter = Vector3Add(position, Vector3Scale(smashChargeDir, 1.8f));
		aoeCenter.y = 0.12f;

		// 1. 鮮やかな水色の外枠線（敵の赤枠と絶対に被らない）
		Color outlineCol = SKYBLUE;
		DrawCylinderWires(aoeCenter, aoeRadius, aoeRadius, 0.05f, 36, outlineCol);
		DrawCylinderWires(aoeCenter, aoeRadius + 0.05f, aoeRadius + 0.05f, 0.05f, 36, Fade(WHITE, 0.8f));

		// 2. 内部の満ちる色（薄いシアン → 濃い青へ）
		Color fillCol = Fade(BLUE, 0.15f + progress * 0.45f);
		DrawCylinder(aoeCenter, aoeRadius, aoeRadius, 0.04f, 36, fillCol);

		// 3. 中心から外枠へ広がるチャージゲージ円（白く輝く水色）
		float gaugeRadius = aoeRadius * progress;
		Color gaugeCol = Fade(SKYBLUE, 0.5f + progress * 0.4f);
		DrawCylinder(aoeCenter, gaugeRadius, gaugeRadius, 0.05f, 36, gaugeCol);
	}


}