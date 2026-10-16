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

Vector3 Player::customWeaponOffsetPos = { -10.0f, 0.0f, 0.0f };
Vector3 Player::customWeaponOffsetRot = { 90.0f, 180.0f, 0.0f };
float Player::customWeaponScale = 100.0f;

static std::string T(const std::string& key, const std::string& def) {
	if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
	return def;
}

static Matrix GetPlayerBoneGlobalMatrix(Model model, ModelAnimation anim, int frame, int boneIndex) {
	if (boneIndex < 0 || boneIndex >= model.boneCount) return MatrixIdentity();
	if (anim.frameCount <= 0 || anim.framePoses == nullptr) return MatrixIdentity();

	if (frame < 0) frame = 0;
	if (frame >= anim.frameCount) frame = anim.frameCount - 1;

	Transform b = anim.framePoses[frame][boneIndex];

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

	critRate = 0.15f;
	critMultiplier = 1.75f;
	armorPenetration = 0.0f;
	statusDamageBonus = 0.25f;

	lifestealRate = 0.0f;
	reflectRate = 0.0f;
	spearSweetBonus = 0.0f;
	axeSweetBonus = 0.0f;
	wandSweetBonus = 0.0f;
	elemPotencyBonus = 0.0f;

	hasGlassCannon = false;
	hasUnstoppableColossus = false;
	hasMasteryMaster = false;
	hasBloodPact = false;
	hasElementalOverload = false;
	hasUndyingWill = false;
	undyingTimer = 0.0f;
	undyingUsed = false;

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
	case ELEM_HELLFIRE: return Color{ 255, 60, 40, 255 };
	case ELEM_ROT:      return Color{ 80, 220, 60, 255 };
	case ELEM_SOUL:     return Color{ 100, 200, 255, 255 };
	case ELEM_ABYSS:    return Color{ 180, 50, 230, 255 };
	default:            return WHITE;
	}
}

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

	if (atkElem == ELEM_ABYSS) return 1.25f;
	if (atkElem == defElem) return 0.60f;

	if (atkElem == ELEM_HELLFIRE && defElem == ELEM_ROT)  return 1.40f;
	if (atkElem == ELEM_ROT && defElem == ELEM_SOUL) return 1.40f;
	if (atkElem == ELEM_SOUL && defElem == ELEM_HELLFIRE) return 1.40f;

	if (atkElem == ELEM_HELLFIRE && defElem == ELEM_SOUL) return 0.75f;
	if (atkElem == ELEM_ROT && defElem == ELEM_HELLFIRE) return 0.75f;
	if (atkElem == ELEM_SOUL && defElem == ELEM_ROT)  return 0.75f;

	return 1.0f;
}

float Player::GetPlayerElementResistance(int elem) {
	if (elem == ELEM_NONE) return 0.0f;
	float totalRes = 0.0f;
	for (int i = 0; i < 5; i++) {
		if (equippedArmor[i].id != -1 && equippedArmor[i].element == elem) {
			totalRes += 0.15f;
		}
	}
	// パッシブ「属性コート」等による耐性加算
	for (const auto& node : skillTree) {
		if (node.unlocked && node.id == 26) totalRes += 0.15f;
	}
	if (totalRes > 0.75f) totalRes = 0.75f; // 最大上限75%
	return totalRes;
}

bool Player::HasStatusImmunity(int element) const {
	auto checkMod = [element](int modId) -> bool {
		if (modId == 0) return false;
		Modifier m = DataManager::GetModifier(modId);
		if (m.immune == 99) return true;
		if (m.immune == element) return true;
		return false;
		};

	for (int i = 0; i < 2; i++) {
		if (equippedData[i].id != -1 && checkMod(equippedData[i].modifierId)) return true;
	}
	for (int i = 0; i < 5; i++) {
		if (equippedArmor[i].id != -1 && checkMod(equippedArmor[i].modifierId)) return true;
	}
	return false;
}

// =============================================================================
// ★ 6段階新パイプライン（キーストーン＆特化パッシブ完全統合）
// =============================================================================
DamageResult Player::CalculateDamageToEnemy(const Enemy& target, float distance, WeaponType wType, bool isSkill) {
	DamageResult res;

	// [ステップ1: 基礎ダメージ算出 ＆ 乱数振れ幅]
	float totalBonus = GetItemTotalAtkBonus(equippedData[activeSlot]);
	float baseAtk = attackPower + totalBonus;
	if (isSkill) baseAtk *= 2.5f;

	// キーストーン5（属性暴走）: 物理基礎-25%
	if (hasElementalOverload) baseAtk *= 0.75f;

	float variance = 0.0f;
	if (wType == SWORD)      variance = (float)GetRandomValue(-5, 5) / 100.0f;
	else if (wType == SPEAR) variance = (float)GetRandomValue(-10, 10) / 100.0f;
	else if (wType == AXE)   variance = (float)GetRandomValue(-25, 35) / 100.0f;
	else if (wType == WAND)  variance = (float)GetRandomValue(-12, 12) / 100.0f;
	else                     variance = (float)GetRandomValue(-5, 5) / 100.0f;

	float baseDmg = baseAtk * (1.0f + variance);

	// [ステップ2: 武器種・間合い補正 (Sweet Spot)]
	float distFactor = 1.0f;
	float masteryBonus = hasMasteryMaster ? 0.25f : 0.0f; // キーストーン3「武芸百般」

	if (wType == SPEAR) {
		if (distance >= 3.8f && distance <= 5.2f) {
			distFactor = 1.40f + spearSweetBonus + masteryBonus;
			res.isSweetSpot = true;
		}
	}
	else if (wType == AXE) {
		if (distance <= 1.8f) {
			distFactor = 1.25f + axeSweetBonus + masteryBonus;
			res.isSweetSpot = true;
		}
	}
	else if (wType == WAND) {
		if (distance >= 5.0f) {
			distFactor = 1.20f + wandSweetBonus + masteryBonus;
			res.isSweetSpot = true;
		}
	}
	else if (wType == SWORD) {
		if (distance >= 1.2f && distance <= 2.2f) {
			distFactor = 1.15f + masteryBonus;
		}
	}

	float preMitigationDamage = baseDmg * distFactor;

	// [ステップ3: 防御減衰 & 貫通]
	float targetDef = target.defense;
	targetDef *= (1.0f - target.status.GetDefenseReductionRate());

	float effectivePen = armorPenetration;
	if (hasElementalOverload) effectivePen += 0.15f; // 属性暴走
	float effectiveDef = targetDef * (1.0f - effectivePen);
	if (effectiveDef < 0.0f) effectiveDef = 0.0f;

	float defMitigation = effectiveDef / (effectiveDef + 100.0f + (float)target.level * 5.0f);
	float postArmorDamage = preMitigationDamage * (1.0f - defMitigation);

	// [ステップ4: 属性相性 & 耐性計算]
	int weaponElem = equippedData[activeSlot].element;
	float elemMulti = GetElementMultiplier(weaponElem, target.data.element);

	// 属性特効ボーナスの加算
	if (elemMulti > 1.1f) {
		if (hasElementalOverload) elemMulti = 2.0f; // 属性暴走時は確定2.0倍
		else elemMulti += elemPotencyBonus;
	}
	res.elemMultiplier = elemMulti;
	float postElementDamage = postArmorDamage * elemMulti;

	// [ステップ5: クリティカル判定]
	float effectiveCritRate = critRate;
	float effectiveCritDmg = critMultiplier;

	if (hasGlassCannon) {
		effectiveCritDmg += 0.50f; // 硝子の大砲: 会心威力+0.5x
		postElementDamage *= 1.40f; // 硝子の大砲: 与ダメ+40%
	}

	if (target.status.chill.timer > 0.0f) effectiveCritRate += 0.25f; // 凍傷時会心率+25%
	if (res.isSweetSpot && wType == SPEAR) effectiveCritRate += 0.30f;
	if (isSkill) effectiveCritRate += 0.10f;

	float roll = (float)GetRandomValue(1, 1000) / 1000.0f;
	if (roll <= effectiveCritRate) {
		res.isCrit = true;
		postElementDamage *= effectiveCritDmg;
	}

	// [ステップ6: 状態異常特効]
	if (target.status.HasAnyStatus()) {
		postElementDamage *= (1.0f + statusDamageBonus);
		res.isStatusPunish = true;
	}

	res.damage = (int)fmaxf(1.0f, roundf(postElementDamage));
	return res;
}

int Player::CompareWithEquipped(const ItemData& item, const Player& p) {
	if (item.id == -1) return 0;

	if (item.type == "EQUIP") {
		const ItemData& cur = p.equippedData[p.activeSlot];
		float curAtk = 0.0f;
		if (cur.id != -1) {
			curAtk = cur.atkBonus + DataManager::GetModifier(cur.modifierId).atk;
		}
		else {
			return 1;
		}

		float itemAtk = item.atkBonus + DataManager::GetModifier(item.modifierId).atk;
		if (itemAtk > curAtk + 0.01f) return 1;
		if (itemAtk < curAtk - 0.01f) return -1;
		return 0;
	}

	if (item.type == "ARMOR") {
		int sub = item.weaponSubtype;
		if (sub < 0 || sub >= 5) return 0;

		const ItemData& cur = p.equippedArmor[sub];
		float curDef = 0.0f;
		if (cur.id != -1) {
			curDef = cur.defBonus + DataManager::GetModifier(cur.modifierId).def;
		}
		else {
			return 1;
		}

		float itemDef = item.defBonus + DataManager::GetModifier(item.modifierId).def;
		if (itemDef > curDef + 0.01f) return 1;
		if (itemDef < curDef - 0.01f) return -1;
		return 0;
	}

	return 0;
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

int Player::GetItemTier(const ItemData& item) {
	if (item.id == -1) return 0;
	if (item.type == "MATERIAL" || item.type == "CONSUMABLE") return 1;

	int tier = 1;
	if (item.id >= 100 && item.id < 200) tier = 1;
	else if (item.id >= 200 && item.id < 300) tier = 2;
	else if (item.id >= 300 && item.id < 400) tier = 3;
	else if (item.id >= 400 && item.id < 500) tier = 4;
	else if (item.id >= 500) tier = 5;

	if (item.modifierId != 0 && tier < 5) tier++;
	return tier;
}

// ★ 全43ノード対応の再計算処理（キーストーン補正の積算）
void Player::RecalculateStats() {
	float bHp = 100.0f + (level - 1) * 20.0f;
	float bAtk = 12.0f + (level - 1) * 2.0f;
	float bDef = 5.0f + (level - 1) * 1.5f;
	float bSpd = 0.18f;
	cooldownReduction = 0.0f;
	healBonus = 0.0f;

	critRate = 0.15f;
	critMultiplier = 1.75f;
	armorPenetration = 0.0f;
	statusDamageBonus = 0.25f;

	lifestealRate = 0.0f;
	reflectRate = 0.0f;
	spearSweetBonus = 0.0f;
	axeSweetBonus = 0.0f;
	wandSweetBonus = 0.0f;
	elemPotencyBonus = 0.0f;

	hasGlassCannon = false;
	hasUnstoppableColossus = false;
	hasMasteryMaster = false;
	hasBloodPact = false;
	hasElementalOverload = false;
	hasUndyingWill = false;

	// スキルツリーのアンロックノード全積算
	for (const auto& node : skillTree) {
		if (node.unlocked) {
			bAtk += node.atkAdd;
			bDef += node.defAdd;
			bHp += node.hpAdd;
			bSpd += node.spdAdd;
			cooldownReduction += node.cdRedAdd;
			healBonus += node.healAdd;

			critRate += node.critRateAdd;
			critMultiplier += node.critDmgAdd;
			armorPenetration += node.armorPenAdd;
			statusDamageBonus += node.statusDmgAdd;
			elemPotencyBonus += node.elemDmgAdd;
			lifestealRate += node.lifestealAdd;
			reflectRate += node.reflectAdd;

			if (node.id == 28) spearSweetBonus += 0.35f;
			if (node.id == 29) axeSweetBonus += 0.25f;
			if (node.id == 38) wandSweetBonus += 0.30f;

			// キーストーンの有効化
			if (node.keystoneType == 1) hasGlassCannon = true;
			if (node.keystoneType == 2) hasUnstoppableColossus = true;
			if (node.keystoneType == 3) hasMasteryMaster = true;
			if (node.keystoneType == 4) hasBloodPact = true;
			if (node.keystoneType == 5) hasElementalOverload = true;
			if (node.keystoneType == 6) hasUndyingWill = true;
		}
	}

	if (zoukyouTimer > 0) bAtk *= 1.5f;
	if (kongoTimer > 0) bDef += 20.0f;

	bAtk *= (1.0f - status.GetAttackReductionRate());
	bDef *= (1.0f - status.GetDefenseReductionRate());

	for (int i = 0; i < 5; i++) if (equippedArmor[i].id != -1) {
		Modifier m = DataManager::GetModifier(equippedArmor[i].modifierId);
		bHp += equippedArmor[i].hpBonus + m.hp; bDef += equippedArmor[i].defBonus + m.def;
		bAtk += equippedArmor[i].atkBonus + m.atk; bSpd += equippedArmor[i].speedBonus + m.spd;

		if (m.id == 4) critRate += 0.05f;
		if (m.id == 6) { critRate += 0.08f; armorPenetration += 0.10f; }
		if (m.id == 7) { critRate += 0.12f; critMultiplier += 0.35f; armorPenetration += 0.15f; }
	}

	// キーストーンの最終割合補正
	if (hasGlassCannon) bHp *= 0.65f;        // 硝子の大砲: 最大HP-35%
	if (hasBloodPact) bHp *= 0.80f;          // 血の契約: 最大HP-20%
	if (hasUnstoppableColossus) {
		bDef *= 1.50f;                       // 不動の巨兵: 常時防御+50%
		bSpd *= 0.80f;                       // 不動の巨兵: 移動速度-20%
	}

	maxHp = bHp; attackPower = bAtk; defense = bDef; baseSpeed = bSpd;
	if (hp > maxHp) hp = maxHp;
}

// =============================================================================
// ★ skills.json からのスキルツリー動的初期化
// =============================================================================
void Player::InitSkillTree() {
	skillTree.clear();
	if (!DataManager::masterSkills.empty()) {
		skillTree = DataManager::masterSkills;
	}
	else {
		// 万が一JSONロード前・未存在時の安全な起点フォールバック
		skillTree.push_back({ 0, T("SKILL_NAME_START", "START"), {600, 400}, {}, true, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, T("SKILL_DESC_START", "Skill Tree Start"), SKILL_PASSIVE, 0.0f });
	}
}

bool Player::IsSkillAvailable(int id) {
	for (auto& n : skillTree) if (n.id == id) {
		if (n.unlocked) return false;
		if (n.reqIds.empty()) return true;
		for (int r : n.reqIds) {
			for (auto& rn : skillTree) {
				if (rn.id == r && rn.unlocked) return true;
			}
		}
	}
	return false;
}

void Player::UnlockSkill(int id) {
	if (IsSkillAvailable(id)) {
		for (auto& n : skillTree) {
			if (n.id == id && skillPoints >= n.cost) {
				skillPoints -= n.cost;
				n.unlocked = true;
				AudioManager::PlaySE(SE_SKILL);
				RecalculateStats();
				return;
			}
		}
	}
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

// =============================================================================
// 通常攻撃の実行処理（吸血・反射・間合いバフ適用）
// =============================================================================
void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
	AudioManager::PlaySE(SE_ATTACK);
	Vector3 origin = Vector3Add(position, { 0, 0.75f, 0 });
	int weaponElem = equippedData[activeSlot].element;

	if (currentWeapon == WAND) {
		float totalBonus = GetItemTotalAtkBonus(equippedData[activeSlot]);
		float wandDmg = (attackPower + totalBonus) * 1.35f;
		fx.SpawnProjectile(origin, ad, 16.0f, 1, true, wandDmg, weaponElem);
	}
	else {
		EffectType type = FX_SLASH;
		Color fxCol = SKYBLUE;
		float atkRange = 2.7f;

		if (currentWeapon == SPEAR) {
			type = FX_THRUST;
			fxCol = SKYBLUE;
			atkRange = 5.0f;
		}
		else if (currentWeapon == AXE) {
			type = FX_SMASH;
			fxCol = ORANGE;
			atkRange = 3.0f;
		}

		if (weaponElem != ELEM_NONE) {
			fxCol = GetElementColor(weaponElem);
		}

		fx.SpawnEffect(origin, ad, type, fxCol, atkRange);

		bool hitAny = false;
		bool anyCrit = false;
		int totalDmgDealt = 0;

		for (auto& e : enemies) {
			float dist = Vector3Distance(e.position, position);
			if (dist <= atkRange + e.radius) {
				if (!d.HasLineOfSight(position, e.position)) continue;

				Vector3 toEnemy = Vector3Normalize(Vector3Subtract(e.position, position));
				float dot = Vector3DotProduct(ad, toEnemy);
				float requiredDot = (currentWeapon == SPEAR) ? 0.6f : 0.0f;

				if (dot >= requiredDot) {
					DamageResult dmgRes = CalculateDamageToEnemy(e, dist, currentWeapon, false);
					if (dmgRes.isCrit) anyCrit = true;

					e.hp -= dmgRes.damage;
					totalDmgDealt += dmgRes.damage;

					// 不動の巨兵（キーストーン2）以外はノックバック適用
					e.ApplyKnockback(ad, dmgRes.isCrit ? 1.8f : 1.0f, d);

					Color hitCol = (weaponElem != ELEM_NONE) ? GetElementColor(weaponElem) : (dmgRes.isCrit ? GOLD : RED);
					fx.SpawnDamageText(e.position, dmgRes.damage, dmgRes.isCrit);
					fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, hitCol);

					if (weaponElem != ELEM_NONE) {
						int statusChance = (dmgRes.elemMultiplier > 1.1f) ? 60 : 35;
						if (GetRandomValue(1, 100) <= statusChance) {
							e.status.ApplyStatus(weaponElem, 3.5f, (float)dmgRes.damage);

							std::string logKey = "";
							if (weaponElem == ELEM_HELLFIRE) logKey = "LOG_STATUS_BURN";
							else if (weaponElem == ELEM_ROT) logKey = "LOG_STATUS_ROT";
							else if (weaponElem == ELEM_SOUL) logKey = "LOG_STATUS_CHILL";
							else if (weaponElem == ELEM_ABYSS) logKey = "LOG_STATUS_CURSE";

							if (!logKey.empty() && DataManager::uiStrings.count(logKey)) {
								UI::AddSystemLog(TextFormat(DataManager::uiStrings[logKey].c_str(), e.data.name.c_str()), hitCol);
							}
						}
					}

					if (dmgRes.isSweetSpot) {
						UI::AddSystemLog(T("LOG_SWEET_SPOT", "★ SWEET SPOT! ★"), GOLD);
					}

					if (dmgRes.elemMultiplier > 1.1f) {
						std::string weakFmt = DataManager::uiStrings.count("LOG_WEAKNESS_HIT") ? DataManager::uiStrings["LOG_WEAKNESS_HIT"] : "★ %s WEAKNESS! %d DMG! ★";
						UI::AddSystemLog(TextFormat(weakFmt.c_str(), e.data.name.c_str(), dmgRes.damage), hitCol);
					}
					else if (dmgRes.elemMultiplier < 0.9f) {
						std::string resFmt = DataManager::uiStrings.count("LOG_RESISTED_ATTACK") ? DataManager::uiStrings["LOG_RESISTED_ATTACK"] : "[RESIST] %s: %d DMG";
						UI::AddSystemLog(TextFormat(resFmt.c_str(), e.data.name.c_str(), dmgRes.damage), LIGHTGRAY);
					}
					else if (dmgRes.isCrit) {
						UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_CRIT_DEALT"].c_str(), e.data.name.c_str(), dmgRes.damage), GOLD);
					}
					else {
						UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_DMG_DEALT"].c_str(), e.data.name.c_str(), dmgRes.damage), ORANGE);
					}

					e.hudTimer = 5.0f;
					isStealth = false;
					hitAny = true;
				}
			}
		}

		// ★ 吸血（ライフスティール）処理
		if (hitAny && totalDmgDealt > 0 && lifestealRate > 0.001f) {
			float lHeal = (float)totalDmgDealt * lifestealRate;
			if (lHeal >= 1.0f) {
				hp = fminf(maxHp, hp + lHeal);
				fx.SpawnEffect(position, { 0, 1, 0 }, FX_HIT, GREEN);
			}
		}

		if (hitAny) {
			if (anyCrit) {
				fx.TriggerHitStop(0.08f);
				fx.ShakeScreen(0.18f, 0.35f);
			}
			else if (currentWeapon == AXE) {
				fx.TriggerHitStop(0.08f);
				fx.ShakeScreen(0.12f, 0.25f);
			}
			else if (currentWeapon == SPEAR) {
				fx.TriggerHitStop(0.05f);
			}
			else if (currentWeapon == SWORD) {
				fx.TriggerHitStop(0.04f);
			}
		}
	}
}

// =============================================================================
// 強撃(SMASH)の実行処理（吸血処理統合）
// =============================================================================
void Player::PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
	AudioManager::PlaySE(SE_SKILL);
	fx.SpawnEffect(Vector3Add(position, { 0, 0.8f, 0 }), ad, FX_SMASH, RED, 4.2f);

	bool hitAny = false;
	bool anyCrit = false;
	int totalDmgDealt = 0;

	for (auto& e : enemies) {
		float dist = Vector3Distance(e.position, position);
		if (dist < 4.5f) {
			if (!d.HasLineOfSight(position, e.position)) continue;

			DamageResult dmgRes = CalculateDamageToEnemy(e, dist, currentWeapon, true);
			if (dmgRes.isCrit) anyCrit = true;

			e.hp -= dmgRes.damage;
			totalDmgDealt += dmgRes.damage;
			e.ApplyKnockback(ad, 3.5f, d);
			fx.SpawnDamageText(e.position, dmgRes.damage, dmgRes.isCrit);
			fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, dmgRes.isCrit ? GOLD : RED);

			if (dmgRes.isCrit) {
				std::string critFmt = DataManager::uiStrings.count("LOG_SMASH_CRIT") ? DataManager::uiStrings["LOG_SMASH_CRIT"] : "★ SMASH CRIT! %s: %d DMG! ★";
				UI::AddSystemLog(TextFormat(critFmt.c_str(), e.data.name.c_str(), dmgRes.damage), GOLD);
			}
			else {
				std::string smashFmt = DataManager::uiStrings.count("LOG_SMASH_HIT") ? DataManager::uiStrings["LOG_SMASH_HIT"] : "SMASH: %s: %d DMG!";
				UI::AddSystemLog(TextFormat(smashFmt.c_str(), e.data.name.c_str(), dmgRes.damage), ORANGE);
			}

			e.hudTimer = 5.0f;
			isStealth = false;
			hitAny = true;
		}
	}

	// 強撃時の吸血処理
	if (hitAny && totalDmgDealt > 0 && lifestealRate > 0.001f) {
		float lHeal = (float)totalDmgDealt * lifestealRate;
		if (lHeal >= 1.0f) {
			hp = fminf(maxHp, hp + lHeal);
			fx.SpawnEffect(position, { 0, 1, 0 }, FX_HIT, GREEN);
		}
	}

	if (hitAny) {
		if (anyCrit) {
			fx.ShakeScreen(0.35f, 0.7f);
			fx.TriggerHitStop(0.14f);
		}
		else {
			fx.ShakeScreen(0.25f, 0.5f);
			fx.TriggerHitStop(0.10f);
		}
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

	// ★ 不撓不屈（キーストーン6: 食いしばり無敵タイマー）
	if (undyingTimer > 0.0f) {
		undyingTimer -= dt;
		if (undyingTimer < 0.0f) undyingTimer = 0.0f;
	}

	// 致命傷時の不撓不屈判定
	if (hp <= 0.0f && hasUndyingWill && !undyingUsed) {
		hp = 1.0f;
		undyingTimer = 3.0f; // 3秒間完全無敵
		undyingUsed = true;
		AudioManager::PlaySE(SE_SAVE);
		fx.SpawnEffect(position, { 0, 1, 0 }, FX_EXPLOSION, GOLD, 3.5f);
		UI::AddSystemLog("★ 不撓不屈発動！ 3秒間無敵！ ★", GOLD);
	}

	bool prevHadStatus = status.HasAnyStatus();
	status.Update(dt, hp, fx, position);
	if (prevHadStatus != status.HasAnyStatus()) {
		RecalculateStats();
	}

	if (isChargingSmash) {
		smashChargeTimer -= dt;
		smashChargeDir = lastAimDir;

		if (smashChargeTimer <= 0.0f) {
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

		curSpd *= status.GetSpeedMultiplier();

		if (isMoving) {
			md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, curSpd);
			if (!d.CheckCollisionRadius(Vector3Add(position, { v.x,0,0 }), radius)) position.x += v.x;
			if (!d.CheckCollisionRadius(Vector3Add(position, { 0,0,v.z }), radius)) position.z += v.z;
			modelRotation = atan2f(md.x, md.z) * RAD2DEG;
		}
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
	if (attackInput && attackTimer <= 0 && !isChargingSmash && currentWeapon != NONE) {
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

	if (btnSmash && IsSkillUnlocked(SKILL_ACTIVE_SMASH) && smashCooldownTimer <= 0 && attackTimer <= 0 && !isChargingSmash) {
		isChargingSmash = true;
		smashChargeMax = 0.6f;
		smashChargeTimer = smashChargeMax;
		smashChargeDir = lastAimDir;
		attackTimer = 0.7f;
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

	if (attackTimer > 0 || isChargingSmash) modelRotation = atan2f(lastAimDir.x, lastAimDir.z) * RAD2DEG;

	int targetAnim = 4;
	if (hp <= 0) targetAnim = 3;
	else if (isChargingSmash) targetAnim = 4;
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

	// 不撓不屈（無敵中）は黄金に光り輝く
	Color pCol = WHITE;
	if (undyingTimer > 0.0f) pCol = (GetRandomValue(0, 10) < 5) ? GOLD : WHITE;
	else if (isStealth) pCol = Fade(BLUE, 0.4f);

	DrawModel(gm.model, drawPos, scale, pCol);

	if (!isDead && status.HasAnyStatus()) {
		status.DrawAura(position, radius);
	}

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

	if (isChargingSmash) {
		float progress = 1.0f - (smashChargeTimer / smashChargeMax);
		if (progress < 0.0f) progress = 0.0f;
		if (progress > 1.0f) progress = 1.0f;

		float aoeRadius = 3.8f;
		Vector3 aoeCenter = Vector3Add(position, Vector3Scale(smashChargeDir, 1.8f));
		aoeCenter.y = 0.12f;

		Color outlineCol = SKYBLUE;
		DrawCylinderWires(aoeCenter, aoeRadius, aoeRadius, 0.05f, 36, outlineCol);
		DrawCylinderWires(aoeCenter, aoeRadius + 0.05f, aoeRadius + 0.05f, 0.05f, 36, Fade(WHITE, 0.8f));

		Color fillCol = Fade(BLUE, 0.15f + progress * 0.45f);
		DrawCylinder(aoeCenter, aoeRadius, aoeRadius, 0.04f, 36, fillCol);

		float gaugeRadius = aoeRadius * progress;
		Color gaugeCol = Fade(SKYBLUE, 0.5f + progress * 0.4f);
		DrawCylinder(aoeCenter, gaugeRadius, gaugeRadius, 0.05f, 36, gaugeCol);
	}
}