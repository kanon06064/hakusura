#pragma once
#include "Definitions.h"

class Player {
public:
    Vector3 position, lastAimDir;
    float speed, radius, attackTimer, visualTimer, hp, maxHp, attackPower, defense;
    int level, exp, expToNext, skillPoints, activeSlot;
    WeaponType equippedWeapons[2], currentWeapon;
    ItemData equippedData[2];
    bool isAttacking;
    std::vector<Projectile> projectiles;
    std::vector<ItemData> inventoryItems;
    std::vector<ItemData> inventoryEquip;

    Player(Vector3 startPos);
    void Update(Camera3D& cam, class Dungeon& d, std::vector<class Enemy>& enemies, std::vector<DamageText>& dt, bool stopMove);
    void Draw();
    void AddExp(int amount, std::vector<DamageText>& dt);
    bool AddToInventory(ItemData item);
    void UseItem(int index);
    void EquipWeapon(int index, int slot);
    void UpgradeStat(int type);
private:
    void PerformAttack(Vector3 ad, std::vector<class Enemy>& enemies, class Dungeon& d, std::vector<DamageText>& dt);
    void LevelUp(std::vector<DamageText>& dt);
};