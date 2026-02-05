#pragma once
#include "Definitions.h"
#include <vector>

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

    // Åyí«â¡ÅzÉXÉLÉãèÛë‘ä«óù
    float dashTimer, dashCooldownTimer;
    float smashCooldownTimer;
    float stealthTimer, stealthCooldownTimer;
    bool isStealth;

    WeaponType equippedWeapons[2], currentWeapon;
    ItemData equippedData[2];
    std::vector<ItemData> inventoryItems;
    std::vector<ItemData> inventoryEquip;
    std::vector<SkillNode> skillTree;

    Player(Vector3 sp);
    void Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop);
    void Draw(bool debug);
    void AddExp(int a, EffectManager& fx);
    bool AddToInventory(ItemData item);
    void UseItem(int idx);
    void EquipWeapon(int invIdx, int slot);
    void UnequipWeapon(int slot);
    void UnlockSkill(int id);
    bool IsSkillAvailable(int id);

    // Åyí«â¡ÅzÉXÉLÉãî≠ìÆÉ`ÉFÉbÉN
    bool IsSkillUnlocked(SkillType type);
    float GetSkillCooldown(SkillType type);
    float GetSkillMaxCooldown(SkillType type);

    static std::string GetFullItemName(const ItemData& item);
    static float GetItemTotalAtkBonus(const ItemData& item);

private:
    void PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx); // Åyí«â¡Åz
    void LevelUp(EffectManager& fx);
    void InitSkillTree();
};