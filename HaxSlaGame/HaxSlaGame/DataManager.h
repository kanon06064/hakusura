#pragma once
#include "Definitions.h"
#include <vector>
#include <map>
#include <string>

class DataManager {
public:
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    static std::map<std::string, std::string> uiStrings;

    static void LoadAllData();
    static EnemyData GetRandomEnemyForFloor(int floor);
    static ItemData GetItemConfigCopy(int id);
};