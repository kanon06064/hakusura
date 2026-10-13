#pragma once
#include "Definitions.h" 
#include "StatusEffect.h"
#include <vector>
#include <string>

class Dungeon;
class Enemy;
class EffectManager;

class Player {
public:
    // --- ステータス・座標関連 ---
    Vector3 position, lastAimDir;
    float speed, baseSpeed, radius, attackTimer, hp, maxHp, attackPower, defense;
    int level, exp, expToNext, skillPoints, activeSlot;
    int gold;
    bool isAttacking;

    // ★ プレイヤーの状態異常マネージャー（炎上・溶解・凍傷・衰弱）
    StatusManager status;

    // --- アニメーション管理変数 ---
    float animTime;
    int currentAnimIndex;
    int prevAnimIndex;
    float blendWeight;
    bool isTransitioning;
    float modelRotation;
    bool isDead;

    // --- スキルとクールダウンの管理 ---
    float dashTimer, dashCooldownTimer;
    float smashCooldownTimer;

    bool isChargingSmash;
    float smashChargeTimer;
    float smashChargeMax;
    Vector3 smashChargeDir;

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

    // --- 装備・インベントリ・スキルツリー・クエスト ---
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

    // ★ モディファイアによる状態異常完全無効化チェック（1:炎上, 2:溶解, 3:凍傷, 4:衰弱, 99:全無効）
    bool HasStatusImmunity(int element) const;

    static int CompareWithEquipped(const ItemData& item, const Player& p);

private:
    void PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void LevelUp(EffectManager& fx);
    void InitSkillTree();
};