#pragma once
#include "Definitions.h"
#include <vector>

// ëOï˚êÈåæ
class Dungeon;
class Enemy;
class EffectManager;

class Player {
public:
    Vector3 position, lastAimDir;
    float speed, radius, attackTimer, hp, maxHp, attackPower, defense;
    int level, exp, expToNext, skillPoints, activeSlot;
    WeaponType equippedWeapons[2], currentWeapon;
    ItemData equippedData[2];
    bool isAttacking;

    std::vector<ItemData> inventoryItems;
    std::vector<ItemData> inventoryEquip;
    std::vector<SkillNode> skillTree;

    Player(Vector3 sp);

    // EffectManagerÇéÛÇØéÊÇÈ
    void Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop);
    void Draw(bool debug);

    void AddExp(int a, EffectManager& fx);
    bool AddToInventory(ItemData item);
    void UseItem(int idx);
    void EquipWeapon(int invIdx, int slot);
    void UnequipWeapon(int slot);
    void UnlockSkill(int id);
    bool IsSkillAvailable(int id);
private:
    void PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void LevelUp(EffectManager& fx);
    void InitSkillTree();
};