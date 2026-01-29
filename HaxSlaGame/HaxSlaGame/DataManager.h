#pragma once
#include "Definitions.h"
#include <nlohmann/json.hpp>
#include <map>
#include <string>

class DataManager {
public:
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    // UIテキスト用のマップを追加
    static std::map<std::string, std::string> uiStrings;

    static void LoadAllData();
    static EnemyData GetRandomEnemyForFloor(int floor);
};