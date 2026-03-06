#pragma once
#include "Definitions.h"
#include <vector>
#include <map>
#include <string>

class Player;

class DataManager {
public:
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    static std::map<std::string, std::string> uiStrings;
    static std::vector<Modifier> modifiers;
    static std::vector<CraftRecipe> recipes;

    static void LoadAllData();
    static EnemyData GetRandomEnemyForFloor(int floor);
    static EnemyData GetBossEnemy();
    static ItemData GetItemConfigCopy(int id);

    static Modifier GetModifier(int id);
    static int GetRandomModifierId();

    static void SaveGame(int slot, Player* p, int currentFloor, int maxReachedFloor, const std::vector<ItemData>& sItems, const std::vector<ItemData>& sEquip);
    static bool LoadGame(int slot, Player* p, int& currentFloor, int& maxReachedFloor, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);

    // データ削除機能
    static void DeleteSaveData(int slot);

    static SaveHeader GetSaveHeader(int slot);

    // 【追加】セーブデータ削除
    static void DeleteSave(int slot);
};