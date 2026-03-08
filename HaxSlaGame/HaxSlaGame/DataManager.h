#pragma once
#include "Definitions.h"
#include <vector>
#include <map>
#include <string>

class Player;

// モデル管理用構造体
struct GameModel {
    Model model;
    Texture2D texture;
    ModelAnimation* anims;
    int animCount;
    bool loaded;
};

class DataManager {
public:
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    static std::map<std::string, std::string> uiStrings;
    static std::vector<Modifier> modifiers;
    static std::vector<CraftRecipe> recipes;

    // モデルデータ管理
    static std::map<std::string, GameModel> loadedModels;

    // 後方互換性のため
    static Model batModel;
    static Texture2D batTexture;
    static ModelAnimation* batAnims;
    static int batAnimCount;
    static bool isBatModelLoaded;

    static void LoadAllData();
    static void UnloadAllData();

    static EnemyData GetRandomEnemyForFloor(int floor);
    // 【変更】階層に応じてボスを決定するため引数を追加
    static EnemyData GetBossEnemy(int floor);

    static ItemData GetItemConfigCopy(int id);

    static Modifier GetModifier(int id);
    static int GetRandomModifierId();

    static void SaveGame(int slot, Player* p, int currentFloor, int maxReachedFloor, const std::vector<ItemData>& sItems, const std::vector<ItemData>& sEquip);
    static bool LoadGame(int slot, Player* p, int& currentFloor, int& maxReachedFloor, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);
    static SaveHeader GetSaveHeader(int slot);

    static void DeleteSave(int slot);
};