#pragma once
#include "Definitions.h"
#include <vector>
#include <unordered_map>
#include <string>

class Player;

// 3Dモデルとそのテクスチャ、アニメーション、事前解決されたボーンインデックス
struct GameModel {
    Model model;
    Texture2D texture;
    ModelAnimation* anims = nullptr;
    int animCount = 0;
    bool loaded = false;

    // 高速化・アタッチ用ボーンインデックス
    int weaponBoneIndex = -1;
    int handBoneIndex = -1;
};

class DataManager {
public:
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    static std::unordered_map<std::string, std::string> uiStrings;
    static std::vector<Modifier> modifiers;
    static std::vector<CraftRecipe> recipes;
    static std::vector<QuestData> quests;
    static std::vector<SkillNode> masterSkills;
    static std::unordered_map<std::string, GameModel> loadedModels;

    static Model batModel;
    static Texture2D batTexture;
    static ModelAnimation* batAnims;
    static int batAnimCount;
    static bool isBatModelLoaded;

    static Model fallbackWeaponModel;
    static Texture2D titleBg;

    static KeyConfig keyConfig;
    static void LoadConfig();
    static void SaveConfig();
    static void ResetConfig();

    static void LoadAllData();
    static void UnloadAllData();

    static EnemyData GetRandomEnemyForFloor(int floor, int dungeonId);
    static EnemyData GetBossEnemy(int floor, int dungeonId);

    static ItemData GetItemConfigCopy(int id);
    static QuestData GetQuestData(int id);
    static Modifier GetModifier(int id);
    static int GetRandomModifierId();

    static void SaveGame(int slot, Player* p, int currentFloor, int currentDungeon, int unlockedDungeon, const std::vector<int>& maxFloors, const std::vector<ItemData>& sItems, const std::vector<ItemData>& sEquip, bool isPortfolio);
    static bool LoadGame(int slot, Player* p, int& currentFloor, int& currentDungeon, int& unlockedDungeon, std::vector<int>& maxFloors, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip, bool& isPortfolio);
    static SaveHeader GetSaveHeader(int slot);

    static void DeleteSave(int slot);

    static void CacheBoneIndices(GameModel& gm);
};