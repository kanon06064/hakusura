#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// =============================================================================
// ゲーム全体定数
// =============================================================================
const int MAX_MAP_WIDTH = 100;    // ダンジョンマップの最大幅
const int MAX_MAP_HEIGHT = 100;   // ダンジョンマップの最大高さ
const float TILE_SIZE = 2.0f;     // 1マスの3D空間でのサイズ
const int MAX_ITEM_TYPES = 20;    // 所持できる消費・素材の最大種類数
const int MAX_ITEM_STACK = 99;    // 1スロットにスタックできる最大数
const int MAX_EQUIP_INV = 20;     // 所持できる装備品の最大数

// =============================================================================
// 列挙型（Enum）定義
// =============================================================================
enum GameState {
    STATE_TITLE,
    STATE_HOME,
    STATE_DUNGEON,
    STATE_GAMEOVER,
    STATE_GAMECLEAR,
    STATE_DEBUG_ROOM
};

enum WeaponType {
    SWORD = 0,
    SPEAR = 1,
    AXE = 2,
    WAND = 3,
    NONE = 4
};

enum ArmorType {
    HEAD = 0,
    CHEST = 1,
    GAUNTLET = 2,
    LEGS = 3,
    BOOTS = 4
};

enum EnemyType {
    E_SWORD = 0,
    E_SPEAR = 1,
    E_AXE = 2,
    E_ARCHER = 3,
    E_MAGE = 4,
    E_TRAP = 5,
    E_PLANT = 6
};

enum EnemyState {
    STATE_PATROL,
    STATE_CHASE,
    STATE_ATTACK
};

enum MenuTab {
    EQUIP,
    SKILL,
    MAP_TAB,
    INVENTORY,
    SYSTEM_TAB,
    OPTION_TAB,
    CONTROL_TAB
};

enum EffectType {
    FX_SLASH,
    FX_THRUST,
    FX_SMASH,
    FX_HIT,
    FX_EXPLOSION
};

enum RoomType {
    RT_NORMAL,
    RT_SMALL,
    RT_LARGE,
    RT_TREASURE,
    RT_HEAL,
    RT_BOSS
};

// ★ 新アクティブスキル（WHIRLWIND, SHADOWSTEP, CATACLYSM）を含む完全定義
enum SkillType {
    SKILL_PASSIVE = 0,
    SKILL_ACTIVE_DASH = 1,
    SKILL_ACTIVE_SMASH = 2,
    SKILL_ACTIVE_STEALTH = 3,
    SKILL_ACTIVE_KONGO = 4,
    SKILL_ACTIVE_ZOUKYOU = 5,
    SKILL_ACTIVE_HEAL = 6,
    SKILL_ACTIVE_WHIRLWIND = 7,  // 旋風斬
    SKILL_ACTIVE_SHADOWSTEP = 8, // 影歩
    SKILL_ACTIVE_CATACLYSM = 9   // 壊滅
};

enum QuestType {
    QUEST_HUNT,
    QUEST_GATHER
};

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

enum ElementType {
    ELEM_NONE = 0,
    ELEM_HELLFIRE = 1,
    ELEM_ROT = 2,
    ELEM_SOUL = 3,
    ELEM_ABYSS = 4
};

// =============================================================================
// コンフィグ構造体
// =============================================================================
struct KeyConfig {
    int moveForward = 87;  // W
    int moveBackward = 83; // S
    int moveLeft = 65;     // A
    int moveRight = 68;    // D
    int dash = 340;        // Left Shift
    int smash = 49;        // 1 (Slot 1)
    int kongo = 50;        // 2 (Slot 2)
    int zoukyou = 51;      // 3 (Slot 3)
    int stealth = 52;      // 4 (Slot 4)
    int heal = 53;         // 5 (Slot 5)
    int swapWeapon = 81;   // Q

    int padDash = 13;      // A/X (Down)
    int padSmash = 14;     // Y/Triangle (Up)
    int padKongo = 5;      // D-Pad Up
    int padZoukyou = 6;    // D-Pad Right
    int padStealth = 7;    // D-Pad Down
    int padHeal = 8;       // D-Pad Left
    int padSwap = 11;      // R1/RB
    int padAttack = 15;    // B/Circle (Right)

    float mouseSensitivity = 1.0f;
    float padSensitivity = 1.0f;
    bool isFullscreen = false;
};

// =============================================================================
// ゲーム内データ構造体
// =============================================================================
struct Projectile {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float radius = 0.0f;
    bool active = false;
    int type = 0;
    bool isPlayer = false;
    float damage = 15.0f;
    int element = ELEM_NONE;
};

struct VisualEffect {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 dir = { 0.0f, 0.0f, 0.0f };
    EffectType type = FX_SLASH;
    float life = 0.0f;
    float maxLife = 0.0f;
    Color color = WHITE;
    float scale = 1.0f;
};

struct DamageText {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    int amount = 0;
    float life = 0.0f;
    bool isCrit = false;
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
    int immune = 0; // 0:なし, 1:炎上, 2:溶解, 3:凍傷, 4:衰弱, 99:全無効
};

struct ItemData {
    int id = -1;
    std::string name = "";
    std::string type = "";
    std::string modelName = "";
    float heal = 0.0f;
    float atkBonus = 0.0f;
    float defBonus = 0.0f;
    float hpBonus = 0.0f;
    float speedBonus = 0.0f;
    float dropChance = 0.0f;
    int weaponSubtype = -1;
    int count = 1;
    int modifierId = 0;
    int element = ELEM_NONE;
};

struct DroppedItem {
    Vector3 pos = { 0.0f, 0.0f, 0.0f };
    Vector3 vel = { 0.0f, 0.0f, 0.0f };
    float rotation = 0.0f;
    ItemData data;
};

struct EnemyData {
    int id = 0;
    std::string name = "";
    std::string modelName = "";
    std::string weaponModelName = "";
    int type = 0;
    float hp = 0.0f;
    float def = 0.0f;
    float speed = 0.0f;
    float detect = 12.0f;
    float atkRange = 2.0f;
    int minFloor = 1;
    int exp = 20;
    std::vector<int> drops;
    int gold = 0;
    int element = ELEM_NONE;
    Color tint = WHITE;
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
    float spdAdd = 0.0f;
    float cdRedAdd = 0.0f;
    float healAdd = 0.0f;

    float critRateAdd = 0.0f;
    float critDmgAdd = 0.0f;
    float armorPenAdd = 0.0f;
    float statusDmgAdd = 0.0f;
    float elemDmgAdd = 0.0f;
    float lifestealAdd = 0.0f;
    float reflectAdd = 0.0f;

    int keystoneType = 0;

    std::string desc = "";
    SkillType type = SKILL_PASSIVE;
    float maxCooldown = 0.0f;
};

struct QuestData {
    int id = 0;
    std::string title = "";
    std::string description = "";
    QuestType type = QUEST_HUNT;
    int targetId = 0;
    int targetCount = 0;
    int rewardGold = 0;
    int rewardItemId = -1;
    int rewardItemCount = 0;
};

struct PlayerQuest {
    int questId = 0;
    int currentCount = 0;
    bool isCompleted = false;
};

struct SaveHeader {
    bool exists = false;
    int playerLevel = 1;
    int floor = 0;
    std::string timestamp = "";
    bool isPortfolioMode = false;
    int unlockedDungeonId = 0;
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

struct HazardArea {
    Vector3 pos = { 0, 0, 0 };
    float radius = 2.0f;
    float duration = 8.0f;
    float maxDuration = 8.0f;
    float tickTimer = 0.5f;
    float dps = 10.0f;
    Color color = GREEN;
    int element = ELEM_ROT;
    bool active = true;
};

struct GatherSpot {
    Vector3 pos = { 0, 0, 0 };
    int materialItemId = 17;
    int count = 1;
    Color color = SKYBLUE;
    bool gathered = false;
    float pulseTime = 0.0f;
};

struct DamageResult {
    int damage = 0;
    bool isCrit = false;
    bool isSweetSpot = false;
    bool isStatusPunish = false;
    float elemMultiplier = 1.0f;
};