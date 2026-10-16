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

    // ★ 6段階パイプライン用ステータス
    float critRate;           // クリティカル率
    float critMultiplier;     // クリティカル倍率
    float armorPenetration;   // 防御貫通率
    float statusDamageBonus;  // 状態異常の敵への追加特効倍率

    // ★ 新設: 特化パッシブ ＆ キーストーン関連ステータス
    float lifestealRate;      // 吸血率 (与ダメージの割合回復)
    float reflectRate;        // 反射率 (被ダメージの割合を敵に跳ね返す)
    float spearSweetBonus;    // 槍先端ヒット時の追加倍率
    float axeSweetBonus;      // 斧至近ヒット時の追加倍率
    float wandSweetBonus;     // 杖遠距離ヒット時の追加倍率
    float elemPotencyBonus;   // 属性弱点特効の追加倍率

    // キーストーン発動フラグ
    bool hasGlassCannon;      // 硝子の大砲 (与ダメ+40%, 会心倍率+0.5x / 最大HP-35%)
    bool hasUnstoppableColossus; // 不動の巨兵 (常時DEF+50%, ノックバック無効 / 速度-20%)
    bool hasMasteryMaster;    // 武芸百般 (全間合い倍率+25%, 命中時速度UP)
    bool hasBloodPact;        // 血の契約 (吸血率大幅UP / 最大HP-20%)
    bool hasElementalOverload;// 属性暴走 (属性特効2.0x, 貫通+15% / 物理攻撃-25%)
    bool hasUndyingWill;      // 不撓不屈 (致命傷時HP1で耐え3秒無敵)
    float undyingTimer;       // 不撓不屈の無敵残り時間
    bool undyingUsed;         // フロア内で使用済みか

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

    WeaponType equippedWeapons[2], currentWeapon;
    ItemData equippedData[2];
    ItemData equippedArmor[5];

    std::vector<ItemData> inventoryItems;
    std::vector<ItemData> inventoryEquip;

    // ★ 43ノードのスキルツリー一覧 (skills.json からコピー)
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

    bool HasStatusImmunity(int element) const;

    DamageResult CalculateDamageToEnemy(const Enemy& target, float distance, WeaponType wType, bool isSkill = false);

    static int CompareWithEquipped(const ItemData& item, const Player& p);

    // ★ skills.json からのスキルツリー初期化
    void InitSkillTree();

private:
    void PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void LevelUp(EffectManager& fx);
};