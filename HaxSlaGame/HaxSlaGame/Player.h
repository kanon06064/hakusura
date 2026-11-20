#pragma once
#include "Definitions.h" 
#include "StatusEffect.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "EffectManager.h"
#include <vector>
#include <string>

class Player {
public:
    Vector3 position, lastAimDir;
    float speed, baseSpeed, radius, attackTimer, hp, maxHp, attackPower, defense;
    int level, exp, expToNext, skillPoints, activeSlot;
    int gold;
    bool isAttacking;

    float critRate;
    float critMultiplier;
    float armorPenetration;
    float statusDamageBonus;

    float lifestealRate;
    float reflectRate;
    float spearSweetBonus;
    float axeSweetBonus;
    float wandSweetBonus;
    float elemPotencyBonus;

    bool hasGlassCannon;
    bool hasUnstoppableColossus;
    bool hasMasteryMaster;
    bool hasBloodPact;
    bool hasElementalOverload;
    bool hasUndyingWill;
    float undyingTimer;
    bool undyingUsed;

    float shadowstepCritBuffTimer;

    SkillType equippedActiveSkills[5];
    float skillCooldownTimers[10];
    float skillMaxCooldowns[10];

    void EquipSkillToSlot(int slotIndex, SkillType type);
    bool TriggerSlotSkill(int slotIndex, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    float GetSlotSkillCooldown(int slotIndex) const;
    float GetSlotSkillMaxCooldown(int slotIndex) const;
    SkillType GetSlotSkillType(int slotIndex) const;

    StatusManager status;

    float animTime;
    int currentAnimIndex;
    int prevAnimIndex;
    float blendWeight;
    bool isTransitioning;
    float modelRotation;
    bool isDead;

    float dashTimer, dashCooldownTimer;
    bool isChargingSmash;
    float smashChargeTimer;
    float smashChargeMax;
    Vector3 smashChargeDir;

    float smashCooldownTimer;
    float stealthTimer, stealthCooldownTimer;
    float kongoTimer, kongoCooldownTimer;
    float zoukyouTimer, zoukyouCooldownTimer;
    float healCooldownTimer;

    float cooldownReduction;
    float healBonus;
    bool isStealth;

    static Vector3 customWeaponOffsetPos;
    static Vector3 customWeaponOffsetRot;
    static float customWeaponScale;

    WeaponType equippedWeapons[2], currentWeapon;
    ItemData equippedData[2];
    ItemData equippedArmor[5];

    std::vector<ItemData> inventoryItems;
    std::vector<ItemData> inventoryEquip;
    std::vector<SkillNode> skillTree;

    std::vector<PlayerQuest> activeQuests;
    std::vector<int> clearedQuests;

    Player(Vector3 sp);

    void Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop);
    void Draw(bool debug);

    float TakeDamage(float rawDmg, int attackElem, Enemy* attacker, EffectManager& fx);
    void ApplyLifesteal(int damageDealt, EffectManager& fx);

    void AddExp(int a, EffectManager& fx);
    bool AddToInventory(ItemData item);
    void UseItem(int idx);

    void EquipWeapon(int invIdx, int slot);
    void UnequipWeapon(int slot);
    void EquipArmor(int invIdx, int slot);
    void UnequipArmor(int slot);

    void UnlockSkill(int id);
    bool IsSkillAvailable(int id);
    bool IsSkillUnlocked(SkillType type);
    float GetSkillCooldown(SkillType type);
    float GetSkillMaxCooldown(SkillType type);

    void RecalculateStats();

    void UpdateHuntQuest(int enemyId);
    bool CheckGatherQuest(int itemId, int requiredCount);
    void CompleteQuest(int questId);

    static std::string GetFullItemName(const ItemData& item);
    static float GetItemTotalAtkBonus(const ItemData& item);
    static Color GetItemRarityColor(const ItemData& item);
    static int GetItemTier(const ItemData& item);

    static std::string GetElementName(int elem);
    static Color GetElementColor(int elem);
    static float GetElementMultiplier(int atkElem, int defElem);
    float GetPlayerElementResistance(int elem);

    bool HasStatusImmunity(int element) const;

    DamageResult CalculateDamageToEnemy(const Enemy& target, float distance, WeaponType wType, bool isSkill = false);
    static int CompareWithEquipped(const ItemData& item, const Player& p);

    void InitSkillTree();

private:
    void PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformWhirlwind(std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformShadowstep(Vector3 aimDir, Dungeon& d, EffectManager& fx);
    void PerformCataclysm(std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);

    void LevelUp(EffectManager& fx);
};