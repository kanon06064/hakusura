#pragma once
#include "Definitions.h" 
#include <vector>
#include <string>

class Dungeon;
class Enemy;
class EffectManager;

class Player {
public:
    Vector3 position, lastAimDir;
    float speed, baseSpeed, radius, attackTimer, hp, maxHp, attackPower, defense;
    int level, exp, expToNext, skillPoints, activeSlot;
    int gold;
    bool isAttacking;

    // --- アニメーション管理変数 (クロスフェード対応版) ---
    float animTime;           // int から float に変更（時間ベースの滑らかな更新用）
    int currentAnimIndex;     // 現在再生中のアニメーション番号
    int prevAnimIndex;        // 遷移前の直前のアニメーション番号
    float blendWeight;        // 0.0(前) ～ 1.0(今) のブレンド率
    bool isTransitioning;     // 現在アニメーションを補間中かどうか
    float modelRotation;      // モデルの回転（度数法）
    bool isDead;              // 死亡フラグ
    // --------------------------------------------------

    float dashTimer, dashCooldownTimer;
    float smashCooldownTimer;
    float stealthTimer, stealthCooldownTimer;
    float kongoTimer, kongoCooldownTimer;
    float zoukyouTimer, zoukyouCooldownTimer;
    float healCooldownTimer;
    float cooldownReduction;
    float healBonus;
    bool isStealth;

    // ★追加: デバッグメニューからリアルタイムで武器の位置・角度・スケールを微調整するための変数
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

private:
    void PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void LevelUp(EffectManager& fx);
    void InitSkillTree();
};