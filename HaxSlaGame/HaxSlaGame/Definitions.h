#pragma once
#include "raylib.h"
#include <string>
#include <vector>

// --- ゲーム全体の定数定義 ---
const int MAX_MAP_WIDTH = 100;    // ダンジョンマップの最大幅
const int MAX_MAP_HEIGHT = 100;   // ダンジョンマップの最大高さ
const float TILE_SIZE = 2.0f;     // 1マスの3D空間でのサイズ
const int MAX_ITEM_TYPES = 20;    // 所持できるアイテム(消費アイテム・素材)の最大種類数
const int MAX_ITEM_STACK = 99;    // 1スロットにスタックできる最大数
const int MAX_EQUIP_INV = 20;     // 所持できる装備品の最大数

// --- ゲーム状態や種別の列挙型 ---
enum GameState { STATE_TITLE, STATE_HOME, STATE_DUNGEON, STATE_GAMEOVER, STATE_GAMECLEAR, STATE_DEBUG_ROOM };
enum WeaponType { SWORD, SPEAR, AXE, WAND, NONE }; // 武器の種別 (0:剣, 1:槍, 2:斧, 3:杖)
enum ArmorType { HEAD, CHEST, GAUNTLET, LEGS, BOOTS }; // 防具の部位
enum EnemyType { E_SWORD, E_SPEAR, E_AXE, E_ARCHER, E_MAGE, E_TRAP }; // 敵の行動タイプ
enum EnemyState { STATE_PATROL, STATE_CHASE, STATE_ATTACK }; // 敵のAIステート（巡回・追跡・攻撃）
enum MenuTab { EQUIP, SKILL, MAP_TAB, INVENTORY, SYSTEM_TAB, OPTION_TAB, CONTROL_TAB }; // メニュー画面のタブ
enum EffectType { FX_SLASH, FX_THRUST, FX_SMASH, FX_HIT }; // エフェクトの種類
enum RoomType { RT_NORMAL, RT_SMALL, RT_LARGE, RT_TREASURE, RT_HEAL, RT_BOSS }; // ダンジョンの部屋の種類
enum SkillType { SKILL_PASSIVE, SKILL_ACTIVE_DASH, SKILL_ACTIVE_SMASH, SKILL_ACTIVE_STEALTH, SKILL_ACTIVE_KONGO, SKILL_ACTIVE_ZOUKYOU, SKILL_ACTIVE_HEAL }; // スキル種別
enum QuestType { QUEST_HUNT, QUEST_GATHER }; // クエスト種別（討伐・収集）
enum SoundType { SE_ATTACK, SE_ENEMY_ATTACK, SE_CLICK, SE_SKILL, SE_STAIRS, SE_SAVE, SE_REFORGE, SE_LEVELUP, SE_HEAL }; // SEの種別
enum MusicType { BGM_TITLE, BGM_HOME, BGM_DUNGEON, BGM_NONE }; // BGMの種別

// --- コンフィグ(設定)情報を保持する構造体 ---
struct KeyConfig {
    // キーボードの設定 (RaylibのKEY_*定数と対応)
    int moveForward = 87; // W
    int moveBackward = 83; // S
    int moveLeft = 65; // A
    int moveRight = 68; // D
    int dash = 340; // Left Shift
    int smash = 49; // 1
    int kongo = 50; // 2
    int zoukyou = 51; // 3
    int stealth = 52; // 4
    int heal = 53; // 5
    int swapWeapon = 81; // Q

    // ゲームパッドの設定 (GAMEPAD_BUTTON_*定数と対応)
    int padDash = 13;     // A/X (下ボタン)
    int padSmash = 14;    // Y/Triangle (上ボタン)
    int padKongo = 5;     // 十字キー上
    int padZoukyou = 6;   // 十字キー右
    int padStealth = 7;   // 十字キー下
    int padHeal = 8;      // 十字キー左
    int padSwap = 11;     // R1/RB
    int padAttack = 15;   // B/Circle (右ボタン)

    // カメラ感度と画面モード設定
    float mouseSensitivity = 1.0f;
    float padSensitivity = 1.0f;
    bool isFullscreen = false;
};

// --- インゲームの各種データを保持する構造体 ---
struct Projectile { Vector3 pos = { 0.0f, 0.0f, 0.0f }; Vector3 vel = { 0.0f, 0.0f, 0.0f }; float radius = 0.0f; bool active = false; int type = 0; bool isPlayer = false; };
struct VisualEffect { Vector3 pos = { 0.0f, 0.0f, 0.0f }; Vector3 dir = { 0.0f, 0.0f, 0.0f }; EffectType type = FX_SLASH; float life = 0.0f; float maxLife = 0.0f; Color color = WHITE; };
struct DamageText { Vector3 pos = { 0.0f, 0.0f, 0.0f }; int amount = 0; float life = 0.0f; };
struct GameLog { std::string message = ""; float life = 0.0f; Color color = WHITE; };
struct Modifier { int id = 0; std::string name = ""; float atk = 0.0f; float def = 0.0f; float hp = 0.0f; float spd = 0.0f; }; // 装備品のエンチャント(接頭辞)データ
struct ItemData { int id = -1; std::string name = ""; std::string type = ""; std::string modelName = ""; float heal = 0.0f; float atkBonus = 0.0f; float defBonus = 0.0f; float hpBonus = 0.0f; float speedBonus = 0.0f; float dropChance = 0.0f; int weaponSubtype = -1; int count = 1; int modifierId = 0; };
struct DroppedItem { Vector3 pos = { 0.0f, 0.0f, 0.0f }; Vector3 vel = { 0.0f, 0.0f, 0.0f }; float rotation = 0.0f; ItemData data; }; // 物理演算で飛び散るドロップアイテム
struct EnemyData { int id = 0; std::string name = ""; std::string modelName = ""; std::string weaponModelName = ""; int type = 0; float hp = 0.0f; float speed = 0.0f; float detect = 12.0f; float atkRange = 2.0f; int minFloor = 1; int exp = 20; std::vector<int> drops; int gold = 0; };
struct SkillNode { int id = 0; std::string name = ""; Vector2 uiPos = { 0.0f, 0.0f }; std::vector<int> reqIds; bool unlocked = false; int cost = 1; float atkAdd = 0.0f; float defAdd = 0.0f; float hpAdd = 0.0f; float cdRedAdd = 0.0f; float healAdd = 0.0f; std::string desc = ""; SkillType type = SKILL_PASSIVE; float maxCooldown = 0.0f; };
struct QuestData { int id = 0; std::string title = ""; std::string description = ""; QuestType type = QUEST_HUNT; int targetId = 0; int targetCount = 0; int rewardGold = 0; int rewardItemId = -1; int rewardItemCount = 0; };
struct PlayerQuest { int questId = 0; int currentCount = 0; bool isCompleted = false; };
struct SaveHeader { bool exists = false; int playerLevel = 1; int floor = 0; std::string timestamp = ""; bool isPortfolioMode = false; int unlockedDungeonId = 0; }; // セーブデータのヘッダー情報（タイトル画面用）
struct CraftMaterial { int itemId = 0; int count = 0; };
struct CraftRecipe { int resultItemId = 0; std::vector<CraftMaterial> materials; int cost = 0; };