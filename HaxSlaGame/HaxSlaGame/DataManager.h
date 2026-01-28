#pragma once
#include "Definitions.h"
#include <nlohmann/json.hpp>

class DataManager {
public:
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    static void LoadAllData();
    static EnemyData GetRandomEnemyForFloor(int floor);
};