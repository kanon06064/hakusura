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

static Matrix GetPlayerBoneMatrix(Model model, ModelAnimation anim, int frame, int boneIndex) {
	if (boneIndex < 0 || boneIndex >= model.boneCount) return MatrixIdentity();
	Transform boneTransform = anim.framePoses[frame][boneIndex];
	Matrix mat = MatrixMultiply(
		MatrixMultiply(
			MatrixScale(boneTransform.scale.x, boneTransform.scale.y, boneTransform.scale.z),
			QuaternionToMatrix(boneTransform.rotation)
		),
		MatrixTranslate(boneTransform.translation.x, boneTransform.translation.y, boneTransform.translation.z)
	);
	return mat;
}

Player::Player(Vector3 sp) : position(sp), baseSpeed(0.15f), radius(0.45f), attackTimer(0), isAttacking(false), lastAimDir({ 1,0,0 }), hp(100), maxHp(100), attackPower(12), defense(5), level(1), exp(0), expToNext(100), skillPoints(0), gold(0) {
	speed = baseSpeed;
	activeSlot = 0; equippedData[0].id = -1; equippedData[1].id = -1;
	equippedWeapons[0] = NONE; equippedWeapons[1] = NONE;

	for (int i = 0; i < 5; i++) equippedArmor[i].id = -1;

	dashTimer = 0; dashCooldownTimer = 0;
	smashCooldownTimer = 0;
	stealthTimer = 0; stealthCooldownTimer = 0;
	isStealth = false;

	animFrameCounter = 0;
	currentAnimIndex = 4; // Idle
	modelRotation = 0.0f;
	isDead = false;

	ItemData s1 = DataManager::GetItemConfigCopy(0);
	ItemData s2 = DataManager::GetItemConfigCopy(100);
	if (s1.id != -1) { equippedData[0] = s1; equippedWeapons[0] = (WeaponType)s1.weaponSubtype; }
	if (s2.id != -1) { equippedData[1] = s2; equippedWeapons[1] = (WeaponType)s2.weaponSubtype; }
	currentWeapon = equippedWeapons[0];
	InitSkillTree();
	RecalculateStats();
}

std::string Player::GetFullItemName(const ItemData& item) {
	if (item.id == -1) return "EMPTY";
	Modifier mod = DataManager::GetModifier(item.modifierId);
	if (mod.name.empty()) return item.name;
	return mod.name + " " + item.name;
}

float Player::GetItemTotalAtkBonus(const ItemData& item) {
	if (item.id == -1) return 0.0f;
	return item.atkBonus + DataManager::GetModifier(item.modifierId).atk;
}

Color Player::GetItemRarityColor(const ItemData& item) {
	if (item.id == -1) return DARKGRAY;
	if (item.type == "MATERIAL") return LIGHTGRAY;
	if (item.type == "CONSUMABLE") return LIME;

	int tier = 0;
	if (item.id >= 100 && item.id < 200) tier = 1;
	else if (item.id >= 200 && item.id < 300) tier = 2;
	else if (item.id >= 300 && item.id < 400) tier = 3;
	else if (item.id >= 400 && item.id < 500) tier = 4;
	else if (item.id >= 500 && item.id < 510) tier = 1;
	else if (item.id >= 510 && item.id < 520) tier = 2;
	else if (item.id >= 520 && item.id < 530) tier = 3;
	else if (item.id >= 530 && item.id < 540) tier = 4;
	else if (item.id >= 540 && item.id < 550) tier = 5;

	if (item.modifierId != 0 && tier < 5) tier++;

	switch (tier) {
	case 1: return WHITE;
	case 2: return GREEN;
	case 3: return SKYBLUE;
	case 4: return PURPLE;
	case 5: return GOLD;
	default: return WHITE;
	}
}

void Player::RecalculateStats() {
	float baseMaxHp = 100.0f + (float)(level - 1) * 20.0f;
	float baseAtk = 12.0f + (float)(level - 1) * 2.0f;
	float baseDef = 5.0f + (float)(level - 1) * 1.5f;
	float spd = 0.15f;

	for (const auto& node : skillTree) {
		if (node.unlocked) { baseAtk += node.atkAdd; baseDef += node.defAdd; baseMaxHp += node.hpAdd; }
	}

	for (int i = 0; i < 5; i++) {
		if (equippedArmor[i].id != -1) {
			Modifier mod = DataManager::GetModifier(equippedArmor[i].modifierId);
			baseMaxHp += equippedArmor[i].hpBonus + mod.hp;
			baseDef += equippedArmor[i].defBonus + mod.def;
			baseAtk += equippedArmor[i].atkBonus + mod.atk;
			spd += equippedArmor[i].speedBonus + mod.spd;
		}
	}

	maxHp = baseMaxHp;
	if (hp > maxHp) hp = maxHp;
	attackPower = baseAtk;
	defense = baseDef;
	baseSpeed = spd;
}

void Player::InitSkillTree() {
	skillTree.push_back({ 0, "START", {640, 650}, {}, true, 0 });
	skillTree.push_back({ 1, "HP I",  {500, 550}, {0}, false, 1, 0, 0, 30, SKILL_PASSIVE });
	skillTree.push_back({ 2, "DEF I", {400, 480}, {1}, false, 1, 0, 3, 0, SKILL_PASSIVE });
	skillTree.push_back({ 3, "STEALTH", {300, 400}, {2}, false, 3, 0, 0, 0, SKILL_ACTIVE_STEALTH, 15.0f });
	skillTree.push_back({ 4, "HP II", {300, 300}, {3}, false, 2, 0, 0, 50, SKILL_PASSIVE });
	skillTree.push_back({ 5, "ATK I", {640, 500}, {0}, false, 1, 5, 0, 0, SKILL_PASSIVE });
	skillTree.push_back({ 6, "SMASH",   {640, 400}, {5}, false, 2, 0, 0, 0, SKILL_ACTIVE_SMASH, 5.0f });
	skillTree.push_back({ 7, "ATK II", {640, 300}, {6}, false, 3, 10, 0, 0, SKILL_PASSIVE });
	skillTree.push_back({ 8, "ATK III",{640, 200}, {7}, false, 5, 20, 0, 0, SKILL_PASSIVE });
	skillTree.push_back({ 9,  "SPD I", {780, 550}, {0}, false, 1, 0, 0, 0, SKILL_PASSIVE });
	skillTree.push_back({ 10, "DASH", {880, 480}, {9}, false, 2, 0, 0, 0, SKILL_ACTIVE_DASH, 3.0f });
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
		AudioManager::PlaySE(SE_SKILL);
		RecalculateStats();
	}
}

bool Player::IsSkillUnlocked(SkillType type) { for (auto& node : skillTree) if (node.type == type && node.unlocked) return true; return false; }
float Player::GetSkillCooldown(SkillType type) {
 switch (type) { case SKILL_ACTIVE_DASH: return dashCooldownTimer; case SKILL_ACTIVE_SMASH: return smashCooldownTimer; case SKILL_ACTIVE_STEALTH: return stealthCooldownTimer; default: return 0.0f; }
}

float Player::GetSkillMaxCooldown(SkillType type) { for (auto& node : skillTree) if (node.type == type) return node.maxCooldown; return 1.0f; }

void Player::AddExp(int a, EffectManager& fx) { exp += a; while (exp >= expToNext) LevelUp(fx); }
void Player::LevelUp(EffectManager& fx) {
	level++; exp -= expToNext; expToNext = (int)((float)expToNext * 1.5f);
	skillPoints += 3; fx.SpawnDamageText(position, 999);
	AudioManager::PlaySE(SE_LEVELUP);
	RecalculateStats(); hp = maxHp;
	if (DataManager::uiStrings.count("LOG_LEVELUP")) UI::AddSystemLog(DataManager::uiStrings["LOG_LEVELUP"], YELLOW);
}

bool Player::AddToInventory(ItemData item) {
	if (item.type == "EQUIP" || item.type == "ARMOR") {
		if ((int)inventoryEquip.size() >= MAX_EQUIP_INV) return false; inventoryEquip.push_back(item);
	}
	else {
		for (auto& i : inventoryItems) if (i.id == item.id && i.count < MAX_ITEM_STACK) { i.count++; return true; }
		if ((int)inventoryItems.size() >= MAX_ITEM_TYPES) return false; inventoryItems.push_back(item);
	}
	return true;
}

void Player::UseItem(int idx) {
	if (idx < 0 || idx >= (int)inventoryItems.size()) return;
	if (inventoryItems[idx].type == "CONSUMABLE") {
		hp = fminf(maxHp, hp + inventoryItems[idx].heal);
		if (DataManager::uiStrings.count("LOG_ITEM_USE")) UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_ITEM_USE"].c_str(), inventoryItems[idx].name.c_str()), SKYBLUE);
		inventoryItems[idx].count--;
		if (inventoryItems[idx].count <= 0) inventoryItems.erase(inventoryItems.begin() + idx);
		AudioManager::PlaySE(SE_HEAL);
	}
}

void Player::EquipWeapon(int invIdx, int slot) {
	if (invIdx < 0 || invIdx >= (int)inventoryEquip.size()) return;
	if (inventoryEquip[invIdx].type != "EQUIP") return;
	if (equippedData[slot].id != -1) inventoryEquip.push_back(equippedData[slot]);
	equippedData[slot] = inventoryEquip[invIdx]; equippedWeapons[slot] = (WeaponType)equippedData[slot].weaponSubtype;
	inventoryEquip.erase(inventoryEquip.begin() + invIdx);
	if (activeSlot == slot) currentWeapon = equippedWeapons[slot];
	AudioManager::PlaySE(SE_CLICK);
	RecalculateStats();
}

void Player::UnequipWeapon(int slot) {
	if (equippedData[slot].id == -1 || (int)inventoryEquip.size() >= MAX_EQUIP_INV) return;
	inventoryEquip.push_back(equippedData[slot]);
	equippedData[slot] = ItemData(); equippedWeapons[slot] = NONE;
	if (activeSlot == slot) currentWeapon = NONE;
	AudioManager::PlaySE(SE_CLICK);
	RecalculateStats();
}

void Player::EquipArmor(int invIdx, int slot) {
	if (invIdx < 0 || invIdx >= (int)inventoryEquip.size()) return;
	if (inventoryEquip[invIdx].type != "ARMOR") return;
	if (inventoryEquip[invIdx].weaponSubtype != slot) return;

	if (equippedArmor[slot].id != -1) inventoryEquip.push_back(equippedArmor[slot]);
	equippedArmor[slot] = inventoryEquip[invIdx];
	inventoryEquip.erase(inventoryEquip.begin() + invIdx);
	AudioManager::PlaySE(SE_CLICK);
	RecalculateStats();
}

void Player::UnequipArmor(int slot) {
	if (equippedArmor[slot].id == -1 || (int)inventoryEquip.size() >= MAX_EQUIP_INV) return;
	inventoryEquip.push_back(equippedArmor[slot]);
	equippedArmor[slot] = ItemData();
	AudioManager::PlaySE(SE_CLICK);
	RecalculateStats();
}

void Player::UpdateHuntQuest(int enemyId) {
	for (auto& q : activeQuests) {
		if (!q.isCompleted) {
			QuestData data = DataManager::GetQuestData(q.questId);
			if (data.type == QUEST_HUNT && data.targetId == enemyId) {
				q.currentCount++;
				if (q.currentCount >= data.targetCount) {
					q.currentCount = data.targetCount;
					q.isCompleted = true;
					AudioManager::PlaySE(SE_LEVELUP);
					if (DataManager::uiStrings.count("LOG_QUEST_OBJECTIVE")) UI::AddSystemLog(DataManager::uiStrings["LOG_QUEST_OBJECTIVE"], GREEN);
				}
			}
		}
	}
}

bool Player::CheckGatherQuest(int itemId, int requiredCount) {
	int count = 0;
	for (const auto& item : inventoryItems) {
		if (item.id == itemId) count += item.count;
	}
	return count >= requiredCount;
}

void Player::CompleteQuest(int questId) {
	for (auto it = activeQuests.begin(); it != activeQuests.end(); ++it) {
		if (it->questId == questId) {
			QuestData data = DataManager::GetQuestData(questId);

			gold += data.rewardGold;
			if (DataManager::uiStrings.count("LOG_QUEST_REPORT")) UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_QUEST_REPORT"].c_str(), data.title.c_str()), YELLOW);
			if (data.rewardGold > 0) {
				if (DataManager::uiStrings.count("LOG_REWARD_GOLD")) UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_REWARD_GOLD"].c_str(), data.rewardGold), GOLD);
			}

			if (data.rewardItemId != -1 && data.rewardItemCount > 0) {
				ItemData rewardItem = DataManager::GetItemConfigCopy(data.rewardItemId);
				rewardItem.count = data.rewardItemCount;
				AddToInventory(rewardItem);
				if (DataManager::uiStrings.count("LOG_REWARD_ITEM")) UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_REWARD_ITEM"].c_str(), rewardItem.name.c_str(), data.rewardItemCount), LIME);
			}

			if (data.type == QUEST_GATHER) {
				int needed = data.targetCount;
				for (auto invIt = inventoryItems.begin(); invIt != inventoryItems.end(); ) {
					if (invIt->id == data.targetId) {
						if (invIt->count <= needed) {
							needed -= invIt->count;
							invIt = inventoryItems.erase(invIt);
						}
						else {
							invIt->count -= needed;
							needed = 0;
							++invIt;
						}
						if (needed <= 0) break;
					}
					else {
						++invIt;
					}
				}
			}

			clearedQuests.push_back(questId);
			activeQuests.erase(it);
			AudioManager::PlaySE(SE_REFORGE);
			break;
		}
	}
}

void Player::Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop) {
	if (stop) return;
	float dt = GetFrameTime();
	if (dashCooldownTimer > 0) dashCooldownTimer -= dt; if (smashCooldownTimer > 0) smashCooldownTimer -= dt; if (stealthCooldownTimer > 0) stealthCooldownTimer -= dt;
	if (dashTimer > 0) dashTimer -= dt; if (stealthTimer > 0) stealthTimer -= dt; else isStealth = false;

	if (IsKeyPressed(KEY_ONE) && IsSkillUnlocked(SKILL_ACTIVE_DASH) && dashCooldownTimer <= 0) {
		dashTimer = 0.2f; dashCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_DASH);
		fx.SpawnEffect(position, { 0.0f,1.0f,0.0f }, FX_HIT, WHITE);
		AudioManager::PlaySE(SE_SKILL);
	}
	if (IsKeyPressed(KEY_TWO) && IsSkillUnlocked(SKILL_ACTIVE_SMASH) && smashCooldownTimer <= 0 && currentWeapon != NONE && attackTimer <= 0) {
		Ray ray = GetMouseRay(GetMousePosition(), cam); if (ray.direction.y != 0) { float t = (position.y - ray.position.y) / ray.direction.y; Vector3 tp = Vector3Add(ray.position, Vector3Scale(ray.direction, t)); lastAimDir = Vector3Normalize(Vector3Subtract(tp, position)); lastAimDir.y = 0; lastAimDir = Vector3Normalize(lastAimDir); }
		PerformSmash(lastAimDir, enemies, d, fx); smashCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_SMASH); attackTimer = 0.8f;
		animFrameCounter = 0;
	}
	if (IsKeyPressed(KEY_THREE) && IsSkillUnlocked(SKILL_ACTIVE_STEALTH) && stealthCooldownTimer <= 0) {
		isStealth = true; stealthTimer = 10.0f; stealthCooldownTimer = GetSkillMaxCooldown(SKILL_ACTIVE_STEALTH);
		fx.SpawnEffect(position, { 0.0f,1.0f,0.0f }, FX_HIT, BLUE);
		AudioManager::PlaySE(SE_SKILL);
	}
	if (IsKeyPressed(KEY_Q)) { activeSlot = 1 - activeSlot; currentWeapon = equippedWeapons[activeSlot]; }

	Vector3 cf = Vector3Normalize(Vector3Subtract(cam.target, cam.position)); cf.y = 0; cf = Vector3Normalize(cf); Vector3 cr = { -cf.z, 0, cf.x }, md = { 0.0f,0.0f,0.0f };
	if (IsKeyDown(KEY_W)) md = Vector3Add(md, cf); if (IsKeyDown(KEY_S)) md = Vector3Subtract(md, cf); if (IsKeyDown(KEY_A)) md = Vector3Subtract(md, cr); if (IsKeyDown(KEY_D)) md = Vector3Add(md, cr);
	float currentSpeed = (dashTimer > 0) ? baseSpeed * 3.0f : baseSpeed;
	if (Vector3Length(md) > 0) { md = Vector3Normalize(md); Vector3 v = Vector3Scale(md, currentSpeed); if (!d.CheckCollisionRadius(Vector3Add(position, { v.x, 0, 0 }), radius)) position.x += v.x; if (!d.CheckCollisionRadius(Vector3Add(position, { 0, 0, v.z }), radius)) position.z += v.z; }

	Ray ray = GetMouseRay(GetMousePosition(), cam); if (ray.direction.y != 0) { float t = (position.y - ray.position.y) / ray.direction.y; Vector3 tp = Vector3Add(ray.position, Vector3Scale(ray.direction, t)); lastAimDir = Vector3Normalize(Vector3Subtract(tp, position)); lastAimDir.y = 0; lastAimDir = Vector3Normalize(lastAimDir); }
	if (attackTimer > 0) attackTimer -= GetFrameTime();

	if (IsMouseButtonPressed(0) && attackTimer <= 0 && currentWeapon != NONE) {
		PerformAttack(lastAimDir, enemies, d, fx);
		attackTimer = 0.5f;
		isAttacking = true;
		animFrameCounter = 0;
	}

	if (attackTimer > 0) {
		if (Vector3Length(lastAimDir) > 0.01f) {
			modelRotation = atan2f(lastAimDir.x, lastAimDir.z) * RAD2DEG;
		}
	}
	else if (Vector3Length(md) > 0.01f) {
		modelRotation = atan2f(md.x, md.z) * RAD2DEG;
	}

	if (hp <= 0) {
		if (currentAnimIndex != 3) { currentAnimIndex = 3; animFrameCounter = 0; isDead = true; }
	}
	else if (attackTimer > 0) {
		int targetAnim = 0;
		if (currentWeapon == SWORD) targetAnim = 0;
		else if (currentWeapon == AXE) targetAnim = 1;
		else if (currentWeapon == SPEAR || currentWeapon == WAND) targetAnim = 2;

		if (currentAnimIndex != targetAnim) {
			currentAnimIndex = targetAnim;
		}
	}
	else if (dashTimer > 0) {
		if (currentAnimIndex != 6) { currentAnimIndex = 6; animFrameCounter = 0; }
	}
	else if (Vector3Length(md) > 0) {
		if (currentAnimIndex != 5) { currentAnimIndex = 5; animFrameCounter = 0; }
	}
	else {
		if (currentAnimIndex != 4) { currentAnimIndex = 4; animFrameCounter = 0; }
	}

	if (hp > 0) animFrameCounter++;
}

void Player::PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
	AudioManager::PlaySE(SE_ATTACK);
	Vector3 attackOrigin = Vector3Add(position, { 0.0f, 0.8f, 0.0f });

	if (currentWeapon == WAND) {
		float speed = 15.0f; int type = 1; fx.SpawnProjectile(attackOrigin, ad, speed, type, true);
	}
	else {
		EffectType type = FX_SLASH; if (currentWeapon == SPEAR) type = FX_THRUST; else if (currentWeapon == AXE) type = FX_SMASH; fx.SpawnEffect(attackOrigin, ad, type, SKYBLUE);
		for (auto& e : enemies) {
			if (!d.HasLineOfSight(position, e.position)) continue; Vector3 v = Vector3Subtract(e.position, position); float dist = Vector3Length(v); bool hit = false; float knk = 0;
 switch (currentWeapon) { case SWORD: if (dist < 4.5f && Vector3DotProduct(ad, Vector3Normalize(v)) > cosf(60 * DEG2RAD)) { hit = true; knk = 0.8f; } break; case SPEAR: { float f = Vector3DotProduct(v, ad); Vector3 side = { -ad.z, 0, ad.x }; float s = fabsf(Vector3DotProduct(v, side)); if (f > 0 && f < 8.2f && s < 1.5f) { hit = true; knk = 2.2f; } } break; case AXE: if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(ad, 3.5f))) < 3.5f) { hit = true; knk = 1.3f; } break; default: break; }
									if (hit) {
										float totalBonus = GetItemTotalAtkBonus(equippedData[activeSlot]);
										int dmg = (int)((attackPower + totalBonus)) + GetRandomValue(-2, 3);
										e.hp -= (float)dmg;
										e.hudTimer = 5;
										e.ApplyKnockback(ad, knk, d);
										fx.SpawnDamageText(e.position, dmg);
										fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, ORANGE);
										isStealth = false;
										stealthTimer = 0;
										if (DataManager::uiStrings.count("LOG_DMG_DEALT")) UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_DMG_DEALT"].c_str(), e.data.name.c_str(), dmg), WHITE);
									}
		}
	}
}

void Player::PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx) {
	AudioManager::PlaySE(SE_SKILL);
	Vector3 attackOrigin = Vector3Add(position, { 0.0f, 0.8f, 0.0f }); fx.SpawnEffect(attackOrigin, ad, FX_SMASH, RED);
	for (auto& e : enemies) {
		if (!d.HasLineOfSight(position, e.position)) continue;
		if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(ad, 3.5f))) < 4.5f) {
			float totalBonus = GetItemTotalAtkBonus(equippedData[activeSlot]);
			int dmg = (int)((attackPower + totalBonus) * 2.5f) + GetRandomValue(5, 10);
			e.hp -= (float)dmg;
			e.hudTimer = 5;
			e.ApplyKnockback(ad, 3.0f, d);
			fx.SpawnDamageText(e.position, dmg);
			fx.SpawnEffect(e.position, { 0,0,0 }, FX_HIT, PURPLE);
			isStealth = false;
			stealthTimer = 0;
			if (DataManager::uiStrings.count("LOG_CRIT_DEALT")) UI::AddSystemLog(TextFormat(DataManager::uiStrings["LOG_CRIT_DEALT"].c_str(), e.data.name.c_str(), dmg), ORANGE);
		}
	}
}

void Player::Draw(bool debug) {
	bool hasModel = false;
	std::string key = "Player";

	if (DataManager::loadedModels.count(key) > 0) {
		hasModel = true;
		GameModel& gm = DataManager::loadedModels[key];

		gm.model.transform = MatrixIdentity();

		if (currentAnimIndex >= gm.animCount) currentAnimIndex = 0;
		ModelAnimation anim = gm.anims[currentAnimIndex];
		int frameCount = anim.frameCount;

		if (isDead) {
			animFrameCounter = frameCount - 1;
		}
		else {
			if (currentAnimIndex == 0 || currentAnimIndex == 1 || currentAnimIndex == 2) {
				if (animFrameCounter >= frameCount) {
					animFrameCounter = frameCount - 1;
				}
			}
			else {
				animFrameCounter = animFrameCounter % frameCount;
			}
		}

		if (gm.animCount > 0) {
			UpdateModelAnimation(gm.model, anim, animFrameCounter);
		}

		float scale = 0.01f;
		Matrix matRotX = MatrixRotateX(-90.0f * DEG2RAD);
		Matrix matRotY = MatrixRotateY(modelRotation * DEG2RAD);
		gm.model.transform = MatrixMultiply(gm.model.transform, matRotX);
		gm.model.transform = MatrixMultiply(gm.model.transform, matRotY);

		DrawModel(gm.model, position, scale, WHITE);

		if (currentWeapon != NONE && !isDead && !isStealth) {
			int handBoneIndex = -1;

			for (int i = 0; i < gm.model.boneCount; i++) {
				std::string bName(gm.model.bones[i].name);
				std::transform(bName.begin(), bName.end(), bName.begin(), ::tolower);
				if (bName == "weapon_r") {
					handBoneIndex = i;
					break;
				}
			}
			if (handBoneIndex == -1) {
				for (int i = 0; i < gm.model.boneCount; i++) {
					std::string bName(gm.model.bones[i].name);
					std::transform(bName.begin(), bName.end(), bName.begin(), ::tolower);
					if (bName.find("weapon") != std::string::npos || bName.find("hand_r") != std::string::npos || bName.find("handright") != std::string::npos) {
						handBoneIndex = i;
						break;
					}
				}
			}

			if (handBoneIndex != -1) {
				Matrix matScale = MatrixScale(scale, scale, scale);
				Matrix matTrans = MatrixTranslate(position.x, position.y, position.z);
				Matrix modelBaseTransform = MatrixMultiply(MatrixMultiply(gm.model.transform, matScale), matTrans);

				Matrix boneMatrix = GetPlayerBoneMatrix(gm.model, anim, animFrameCounter, handBoneIndex);
				Matrix boneWorldTransform = MatrixMultiply(boneMatrix, modelBaseTransform);

				int equipId = equippedData[activeSlot].id;
				std::string wpnKey = "Wpn_" + std::to_string(equipId);

				// ★修正: 最高レアリティ（機神シリーズ: ID400番台）の判定
				bool isLegendary = (equipId >= 400 && equipId < 500);

				std::string baseKey = "";
				if (currentWeapon == SWORD) baseKey = "Wpn_Sword";
				else if (currentWeapon == AXE) baseKey = "Wpn_Axe";
				else if (currentWeapon == SPEAR) baseKey = "Wpn_Spear";
				else if (currentWeapon == WAND) baseKey = "Wpn_Wand";

				std::string legendKey = baseKey + "_Legend";

				std::string finalWpnKey = "";
				// 1. 個別IDのモデル（Wpn_401.objなど）があれば最優先
				if (DataManager::loadedModels.count(wpnKey) > 0) finalWpnKey = wpnKey;
				// 2. ID別が無く、最高レアなら Wpn_Sword_Legend.obj などを探す
				else if (isLegendary && DataManager::loadedModels.count(legendKey) > 0) finalWpnKey = legendKey;
				// 3. それも無ければ汎用の Wpn_Sword.obj などにフォールバック
				else if (DataManager::loadedModels.count(baseKey) > 0) finalWpnKey = baseKey;

				if (!finalWpnKey.empty()) {
					GameModel& wGm = DataManager::loadedModels[finalWpnKey];
					wGm.model.transform = boneWorldTransform;
					DrawModel(wGm.model, { 0,0,0 }, 1.0f, WHITE);
					wGm.model.transform = MatrixIdentity();
				}
				else if (debug) {
					Vector3 boneWorldPos = { boneWorldTransform.m12, boneWorldTransform.m13, boneWorldTransform.m14 };
					DrawCube(boneWorldPos, 0.1f, 0.5f, 0.1f, GRAY);
				}
			}
		}
		gm.model.transform = MatrixIdentity();

	}
	else {
		Color bodyColor = RED; if (isStealth) bodyColor = Fade(BLUE, 0.3f);
		DrawCube(position, 1, 1, 1, bodyColor); DrawCubeWires(position, 1, 1, 1, MAROON);
		if (currentWeapon != NONE) { DrawLine3D(position, Vector3Add(position, Vector3Scale(lastAimDir, 3.0f)), Fade(YELLOW, 0.3f)); }
	}
}