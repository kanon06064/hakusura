#pragma once
#include "raylib.h"
#include <vector>

class Dungeon;
class Enemy;
struct DamageText { Vector3 pos; int amount; float life; };
struct Projectile;

enum WeaponType { SWORD, SPEAR, AXE, BOW, WAND };

class Player {
public:
    Vector3 position, lastAimDir;
    float speed, radius, attackTimer, visualTimer, hp, maxHp;
    WeaponType currentWeapon;
    bool isAttacking;
    std::vector<Projectile> projectiles;

    Player(Vector3 startPos);
    void Update(Camera3D& camera, Dungeon& dungeon, std::vector<Enemy>& enemies, std::vector<DamageText>& dmgTexts);
    void Draw();

private:
    void PerformAttack(Vector3 aimDir, std::vector<Enemy>& enemies, Dungeon& dungeon, std::vector<DamageText>& dmgTexts);
};