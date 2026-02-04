#pragma once
#include "raylib.h"
#include <vector>
#include <string>
#include <map>

// 定数
const int MAP_WIDTH = 40;
const int MAP_HEIGHT = 40;
const float TILE_SIZE = 2.0f;
const int MAX_ITEM_TYPES = 20;
const int MAX_ITEM_STACK = 99;
const int MAX_EQUIP_INV = 20;

// 列挙型
enum GameState { STATE_HOME, STATE_DUNGEON };
enum WeaponType { SWORD, SPEAR, AXE, BOW, WAND, NONE };
enum EnemyType { E_SWORD, E_SPEAR, E_AXE, E_ARCHER, E_MAGE, E_TRAP };
enum EnemyState { STATE_PATROL, STATE_CHASE, STATE_ATTACK };
enum MenuTab { EQUIP, SKILL, MAP_TAB, INVENTORY, DEBUG_TAB };
enum EffectType { FX_SLASH, FX_THRUST, FX_SMASH, FX_HIT };

// 構造体
struct Projectile {
    Vector3 pos;
    Vector3 vel;
    float radius;
    bool active;
    int type; // 0:Arrow, 1:Magic
    bool isPlayer; // trueならプレイヤー発射、falseなら敵発射
};

struct VisualEffect {
    Vector3 pos;
    Vector3 dir;
    EffectType type;
    float life;
    float maxLife;
    Color color;
};

struct DamageText {
    Vector3 pos;
    int amount;
    float life;
};

struct GameLog {
    std::string message;
    float life;
    Color color;
};

struct ItemData {
    int id = -1;
    std::string name = "";
    std::string type = ""; // "CONSUMABLE", "MATERIAL", "EQUIP"
    float heal = 0.0f, atkBonus = 0.0f, dropChance = 0.0f;
    int weaponSubtype = -1;
    int count = 1;
};

struct DroppedItem {
    Vector3 pos;
    ItemData data;
};

struct EnemyData {
    int id = 0; std::string name = ""; int type = 0;
    float hp = 0, speed = 0, detect = 12.0f, atkRange = 2.0f;
    int minFloor = 1, exp = 20; std::vector<int> drops;
};

struct SkillNode {
    int id; std::string name; Vector2 uiPos; std::vector<int> reqIds;
    bool unlocked = false; int cost = 1;
    float atkAdd = 0, defAdd = 0, hpAdd = 0;
};