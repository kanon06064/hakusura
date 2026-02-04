#include "DataManager.h"
#include "raylib.h"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

std::vector<EnemyData> DataManager::allEnemyData;
std::vector<ItemData> DataManager::itemConfigs;
std::map<std::string, std::string> DataManager::uiStrings;

void DataManager::LoadAllData() {
    using json = nlohmann::json;
    try {
        std::ifstream iF("items.json");
        if (iF.is_open()) {
            json j; iF >> j;
            itemConfigs.clear();
            for (auto& i : j) {
                ItemData d;
                d.id = i["id"]; d.name = i["name"]; d.type = i["type"];
                d.dropChance = i.value("dropChance", 0.0f);
                d.heal = i.value("heal", 0.0f); d.atkBonus = i.value("atkBonus", 0.0f);
                d.weaponSubtype = i.value("weaponSubtype", -1); d.count = 1;
                itemConfigs.push_back(d);
            }
        }
        std::ifstream eF("enemies.json");
        if (eF.is_open()) {
            json j; eF >> j;
            allEnemyData.clear();
            for (auto& i : j) {
                EnemyData d; d.id = i["id"]; d.name = i["name"]; d.type = i["type"];
                d.hp = i["hp"]; d.speed = i["speed"]; d.minFloor = i["minFloor"];
                d.atkRange = i["atkRange"]; d.exp = i["exp"];
                if (i.contains("drops")) d.drops = i["drops"].get<std::vector<int>>();
                allEnemyData.push_back(d);
            }
        }
        std::ifstream uF("ui_text.json");
        if (uF.is_open()) {
            json j; uF >> j;
            uiStrings.clear();
            for (auto& el : j.items()) uiStrings[el.key()] = el.value().get<std::string>();
        }
    }
    catch (const std::exception& e) {
        std::cerr << "JSON Load Error: " << e.what() << std::endl;
    }
}

EnemyData DataManager::GetRandomEnemyForFloor(int floor) {
    std::vector<EnemyData> c;
    for (auto& e : allEnemyData) if (floor >= e.minFloor) c.push_back(e);
    // データがない場合のフォールバック
    return c.empty() ? EnemyData{ 0, "Slime", 0, 30, 0.05f } : c[GetRandomValue(0, (int)c.size() - 1)];
}

ItemData DataManager::GetItemConfigCopy(int id) {
    for (auto& cfg : itemConfigs) if (cfg.id == id) return cfg;
    ItemData empty; empty.id = -1; return empty;
}