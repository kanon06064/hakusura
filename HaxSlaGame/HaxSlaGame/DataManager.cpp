#include "DataManager.h"
#include <fstream>

std::vector<EnemyData> DataManager::allEnemyData;
std::vector<ItemData> DataManager::itemConfigs;
void DataManager::LoadAllData() {
    using json = nlohmann::json;
    std::ifstream eF("enemies.json"); if (eF.is_open()) { json j; eF >> j; for (auto& i : j) { EnemyData d; d.id = i["id"]; d.name = i["name"]; d.type = i["type"]; d.hp = i["hp"]; d.speed = i["speed"]; d.minFloor = i["minFloor"]; d.atkRange = i["atkRange"]; d.exp = i["exp"]; if (i.contains("drops")) d.drops = i["drops"].get<std::vector<int>>(); allEnemyData.push_back(d); } }
    std::ifstream iF("items.json"); if (iF.is_open()) { json j; iF >> j; for (auto& i : j) { ItemData d; d.id = i["id"]; d.name = i["name"]; d.type = i["type"]; d.dropChance = i["dropChance"]; d.heal = i.value("heal", 0.0f); d.atkBonus = i.value("atkBonus", 0.0f); d.weaponSubtype = i.value("weaponSubtype", -1); itemConfigs.push_back(d); } }
}
EnemyData DataManager::GetRandomEnemyForFloor(int floor) {
    std::vector<EnemyData> c; for (auto& e : allEnemyData) if (floor >= e.minFloor) c.push_back(e);
    return c.empty() ? allEnemyData[0] : c[GetRandomValue(0, (int)c.size() - 1)];
}