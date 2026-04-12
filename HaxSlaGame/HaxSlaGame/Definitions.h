#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// 定数
const int MAX_MAP_WIDTH = 100;
const int MAX_MAP_HEIGHT = 100;
const float TILE_SIZE = 2.0f;
const int MAX_ITEM_TYPES = 20;
const int MAX_ITEM_STACK = 99;
const int MAX_EQUIP_INV = 20;

// 列挙型
enum GameState { STATE_TITLE, STATE_HOME, STATE_DUNGEON, STATE_GAMEOVER, STATE_GAMECLEAR, STATE_DEBUG_ROOM };
enum WeaponType { SWORD, SPEAR, AXE, BOW, WAND, NONE };
enum ArmorType { HEAD, CHEST, GAUNTLET, LEGS, BOOTS };
enum EnemyType { E_SWORD, E_SPEAR, E_AXE, E_ARCHER, E_MAGE, E_TRAP };
enum EnemyState { STATE_PATROL, STATE_CHASE, STATE_ATTACK };
enum MenuTab { EQUIP, SKILL, MAP_TAB, INVENTORY, DEBUG_TAB, SYSTEM_TAB, OPTION_TAB };
enum EffectType { FX_SLASH, FX_THRUST, FX_SMASH, FX_HIT };
enum RoomType { RT_NORMAL, RT_SMALL, RT_LARGE, RT_TREASURE, RT_HEAL, RT_BOSS };
enum SkillType { SKILL_PASSIVE, SKILL_ACTIVE_DASH, SKILL_ACTIVE_SMASH, SKILL_ACTIVE_STEALTH };

// サウンド
enum SoundType {
    SE_ATTACK,
    SE_ENEMY_ATTACK,
    SE_CLICK,
    SE_SKILL,
    SE_STAIRS,
    SE_SAVE,
    SE_REFORGE,
    SE_LEVELUP,
    SE_HEAL
};

enum MusicType {
    BGM_TITLE,
    BGM_HOME,
    BGM_DUNGEON,
    BGM_NONE
};

// 構造体
struct Projectile {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float radius = 0.0f;
    bool active = false;
    int type = 0;
    bool isPlayer = false;
};

struct VisualEffect {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 dir = { 0.0f, 0.0f, 0.0f };
    EffectType type = FX_SLASH;
    float life = 0.0f;
    float maxLife = 0.0f;
    Color color = WHITE;
};

struct DamageText {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    int amount = 0;
    float life = 0.0f;
};

struct GameLog {
    std::string message = "";
    float life = 0.0f;
    Color color = WHITE;
};

struct Modifier {
    int id = 0;
    std::string name = "";
    float atk = 0.0f;
    float def = 0.0f;
    float hp = 0.0f;
    float spd = 0.0f;
};

struct ItemData {
    int id = -1;
    std::string name = "";
    std::string type = "";
    float heal = 0.0f;
    float atkBonus = 0.0f;
    float defBonus = 0.0f;
    float hpBonus = 0.0f;
    float speedBonus = 0.0f;
    float dropChance = 0.0f;
    int weaponSubtype = -1;
    int count = 1;
    int modifierId = 0;
};

struct DroppedItem {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    ItemData data;
};

struct EnemyData {
    int id = 0;
    std::string name = "";
    std::string modelName = "";
    std::string weaponModelName = "";
    int type = 0;
    float hp = 0.0f;
    float speed = 0.0f;
    float detect = 12.0f;
    float atkRange = 2.0f;
    int minFloor = 1;
    int exp = 20;
    std::vector<int> drops;
    int gold = 0;
};

struct SkillNode {
    int id = 0;
    std::string name = "";
    Vector2 uiPos = { 0.0f, 0.0f };
    std::vector<int> reqIds;
    bool unlocked = false;
    int cost = 1;
    float atkAdd = 0.0f;
    float defAdd = 0.0f;
    float hpAdd = 0.0f;
    SkillType type = SKILL_PASSIVE;
    float maxCooldown = 0.0f;
};

struct SaveHeader {
    bool exists = false;
    int playerLevel = 1;
    int floor = 0;
    std::string timestamp = "";
    bool isPortfolioMode = false; // ★追加：ラッシュモード判別用フラグ
};

struct CraftMaterial {
    int itemId = 0;
    int count = 0;
};

struct CraftRecipe {
    int resultItemId = 0;
    std::vector<CraftMaterial> materials;
    int cost = 0;
};