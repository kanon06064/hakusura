#pragma once
#include "Definitions.h"
#include <nlohmann/json.hpp>

class DataManager {
public:
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    static std::map<std::string, std::string> uiStrings;
    static void LoadAllData();
    static EnemyData GetRandomEnemyForFloor(int floor);
};