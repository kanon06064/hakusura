#include "DataManager.h"
#include <fstream>
#include <iostream>

std::vector<EnemyData> DataManager::allEnemyData;
std::vector<ItemData> DataManager::itemConfigs;
std::map<std::string, std::string> DataManager::uiStrings;

void DataManager::LoadAllData() {
    using json = nlohmann::json;
    try {
        // 1. エネミーデータ (enemies.json)
        std::ifstream eF("enemies.json");
        if (eF.is_open()) {
            json j; eF >> j;
            allEnemyData.clear();
            for (auto& i : j) {
                EnemyData d;
                d.id = i.value("id", 0);
                d.name = i.value("name", "Unknown"); // ここに日本語が入る
                d.type = i.value("type", 0);
                d.hp = i.value("hp", 30.0f);
                d.speed = i.value("speed", 0.05f);
                d.minFloor = i.value("minFloor", 1);
                d.atkRange = i.value("atkRange", 2.0f);
                d.exp = i.value("exp", 20);
                if (i.contains("drops")) d.drops = i["drops"].get<std::vector<int>>();
                allEnemyData.push_back(d);
            }
        }

        // 2. アイテムデータ (items.json)
        std::ifstream iF("items.json");
        if (iF.is_open()) {
            json j; iF >> j;
            itemConfigs.clear();
            for (auto& i : j) {
                ItemData d;
                d.id = i.value("id", 0);
                d.name = i.value("name", "Item");
                d.type = i.value("type", "MATERIAL");
                d.dropChance = i.value("dropChance", 0.1f);
                d.heal = i.value("heal", 0.0f);
                d.atkBonus = i.value("atkBonus", 0.0f);
                d.weaponSubtype = i.value("weaponSubtype", -1);
                itemConfigs.push_back(d);
            }
        }

        // 3. UIテキストデータ (ui_text.json)
        std::ifstream uiF("ui_text.json");
        if (uiF.is_open()) {
            json j; uiF >> j;
            for (auto& el : j.items()) {
                uiStrings[el.key()] = el.value().get<std::string>();
            }
        }
    }
    catch (const std::exception& e) {
        std::cerr << "JSON Load Error: " << e.what() << std::endl;
    }
}

EnemyData DataManager::GetRandomEnemyForFloor(int floor) {
    std::vector<EnemyData> c;
    for (auto& e : allEnemyData) if (floor >= e.minFloor) c.push_back(e);
    return c.empty() ? allEnemyData[0] : c[GetRandomValue(0, (int)c.size() - 1)];
}