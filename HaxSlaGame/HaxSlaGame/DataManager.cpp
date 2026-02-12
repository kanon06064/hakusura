#include "DataManager.h"
#include "Player.h"
#include "raylib.h"
#include <fstream>
#include <iostream>
#include <cstdio> // remove—p
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::vector<EnemyData> DataManager::allEnemyData;
std::vector<ItemData> DataManager::itemConfigs;
std::map<std::string, std::string> DataManager::uiStrings;
std::vector<Modifier> DataManager::modifiers;
std::vector<CraftRecipe> DataManager::recipes;

static json ItemToJson(const ItemData& item) {
    return {
        {"id", item.id}, {"name", item.name}, {"type", item.type},
        {"heal", item.heal}, {"atkBonus", item.atkBonus},
        {"defBonus", item.defBonus}, {"hpBonus", item.hpBonus}, {"speedBonus", item.speedBonus},
        {"weaponSubtype", item.weaponSubtype}, {"count", item.count}, {"modifierId", item.modifierId}
    };
}

static ItemData JsonToItem(const json& j) {
    ItemData d;
    d.id = j.value("id", -1); d.name = j.value("name", ""); d.type = j.value("type", "");
    d.heal = j.value("heal", 0.0f); d.atkBonus = j.value("atkBonus", 0.0f);
    d.defBonus = j.value("defBonus", 0.0f); d.hpBonus = j.value("hpBonus", 0.0f);
    d.speedBonus = j.value("speedBonus", 0.0f);
    d.weaponSubtype = j.value("weaponSubtype", -1); d.count = j.value("count", 1);
    d.modifierId = j.value("modifierId", 0);
    return d;
}

void DataManager::LoadAllData() {
    try {
        std::ifstream iF("items.json");
        if (iF.is_open()) {
            json j; iF >> j; itemConfigs.clear();
            for (auto& i : j) itemConfigs.push_back(JsonToItem(i));
        }
        std::ifstream rF("recipes.json");
        if (rF.is_open()) {
            json j; rF >> j; recipes.clear();
            for (auto& r : j) {
                CraftRecipe cr; cr.resultItemId = r.value("resultId", 0); cr.cost = r.value("cost", 0);
                if (r.contains("materials")) for (auto& m : r["materials"]) cr.materials.push_back({ m.value("id", 0), m.value("count", 1) });
                recipes.push_back(cr);
            }
        }
        std::ifstream eF("enemies.json");
        if (eF.is_open()) {
            json j; eF >> j; allEnemyData.clear();
            for (auto& i : j) {
                EnemyData d; d.id = i.value("id", 0); d.name = i.value("name", ""); d.type = i.value("type", 0);
                d.hp = i.value("hp", 0.0f); d.speed = i.value("speed", 0.0f); d.minFloor = i.value("minFloor", 1);
                d.atkRange = i.value("atkRange", 2.0f); d.exp = i.value("exp", 0); d.gold = i.value("gold", 0);
                if (i.contains("drops")) d.drops = i["drops"].get<std::vector<int>>();
                allEnemyData.push_back(d);
            }
        }
        std::ifstream uF("ui_text.json");
        if (uF.is_open()) {
            json j; uF >> j; uiStrings.clear();
            for (auto& el : j.items()) uiStrings[el.key()] = el.value().get<std::string>();
        }
        std::ifstream mF("modifiers.json");
        if (mF.is_open()) {
            json j; mF >> j; modifiers.clear();
            for (auto& i : j) {
                Modifier m; m.id = i.value("id", 0); m.name = i.value("name", "");
                m.atk = i.value("atk", 0.0f); m.def = i.value("def", 0.0f);
                m.hp = i.value("hp", 0.0f); m.spd = i.value("spd", 0.0f);
                modifiers.push_back(m);
            }
        }
    }
    catch (const std::exception& e) { std::cerr << "JSON Load Error: " << e.what() << std::endl; }
}

EnemyData DataManager::GetRandomEnemyForFloor(int floor) {
    std::vector<EnemyData> c; for (auto& e : allEnemyData) if (floor >= e.minFloor) c.push_back(e);
    return c.empty() ? EnemyData{ 0, "Slime", 0, 30, 0.05f } : c[GetRandomValue(0, (int)c.size() - 1)];
}
EnemyData DataManager::GetBossEnemy() {
    if (allEnemyData.empty()) return { 0, "Boss", 0, 1000, 0.1f };
    EnemyData boss = allEnemyData[0]; for (auto& e : allEnemyData) if (e.minFloor > boss.minFloor) boss = e;
    return boss;
}
ItemData DataManager::GetItemConfigCopy(int id) {
    for (auto& cfg : itemConfigs) if (cfg.id == id) return cfg;
    ItemData empty; empty.id = -1; return empty;
}
Modifier DataManager::GetModifier(int id) {
    for (const auto& mod : modifiers) if (mod.id == id) return mod;
    for (const auto& mod : modifiers) if (mod.id == 0) return mod;
    return { 0, "", 0,0,0,0 };
}
int DataManager::GetRandomModifierId() {
    if (modifiers.size() <= 1) return 0;
    return modifiers[GetRandomValue(1, (int)modifiers.size() - 1)].id;
}

void DataManager::SaveGame(int slot, Player* p, int currentFloor, int maxReachedFloor, const std::vector<ItemData>& sItems, const std::vector<ItemData>& sEquip) {
    json root; root["floor"] = currentFloor; root["maxReachedFloor"] = maxReachedFloor;
    root["player"] = {
        {"level", p->level}, {"exp", p->exp}, {"expToNext", p->expToNext},
        {"hp", p->hp}, {"maxHp", p->maxHp}, {"attackPower", p->attackPower},
        {"defense", p->defense}, {"skillPoints", p->skillPoints}, {"gold", p->gold}
    };
    root["inventoryItems"] = json::array(); for (const auto& item : p->inventoryItems) root["inventoryItems"].push_back(ItemToJson(item));
    root["inventoryEquip"] = json::array(); for (const auto& item : p->inventoryEquip) root["inventoryEquip"].push_back(ItemToJson(item));
    root["equipped0"] = ItemToJson(p->equippedData[0]); root["equipped1"] = ItemToJson(p->equippedData[1]);
    root["armor"] = json::array(); for (int i = 0; i < 5; i++) root["armor"].push_back(ItemToJson(p->equippedArmor[i]));
    root["unlockedSkills"] = json::array(); for (const auto& node : p->skillTree) if (node.unlocked) root["unlockedSkills"].push_back(node.id);
    root["storageItems"] = json::array(); for (const auto& item : sItems) root["storageItems"].push_back(ItemToJson(item));
    root["storageEquip"] = json::array(); for (const auto& item : sEquip) root["storageEquip"].push_back(ItemToJson(item));
    std::ofstream o("save" + std::to_string(slot) + ".json"); o << root.dump(4);
}

bool DataManager::LoadGame(int slot, Player* p, int& currentFloor, int& maxReachedFloor, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip) {
    std::ifstream i("save" + std::to_string(slot) + ".json"); if (!i.is_open()) return false;
    try {
        json root; i >> root; currentFloor = root.value("floor", 0); maxReachedFloor = root.value("maxReachedFloor", 0);
        auto& jp = root["player"];
        p->level = jp.value("level", 1); p->exp = jp.value("exp", 0); p->expToNext = jp.value("expToNext", 100);
        p->hp = jp.value("hp", 100.0f); p->maxHp = jp.value("maxHp", 100.0f);
        p->attackPower = jp.value("attackPower", 10.0f); p->defense = jp.value("defense", 0.0f);
        p->skillPoints = jp.value("skillPoints", 0); p->gold = jp.value("gold", 0);
        p->inventoryItems.clear(); for (const auto& jItem : root["inventoryItems"]) p->inventoryItems.push_back(JsonToItem(jItem));
        p->inventoryEquip.clear(); for (const auto& jItem : root["inventoryEquip"]) p->inventoryEquip.push_back(JsonToItem(jItem));
        p->equippedData[0] = JsonToItem(root["equipped0"]); p->equippedWeapons[0] = (p->equippedData[0].id == -1) ? NONE : (WeaponType)p->equippedData[0].weaponSubtype;
        p->equippedData[1] = JsonToItem(root["equipped1"]); p->equippedWeapons[1] = (p->equippedData[1].id == -1) ? NONE : (WeaponType)p->equippedData[1].weaponSubtype;
        p->currentWeapon = p->equippedWeapons[p->activeSlot];
        if (root.contains("armor")) { int idx = 0; for (const auto& jItem : root["armor"]) { if (idx < 5) p->equippedArmor[idx] = JsonToItem(jItem); idx++; } }
        std::vector<int> unlockedIds = root.value("unlockedSkills", std::vector<int>());
        for (int uid : unlockedIds) for (auto& node : p->skillTree) if (node.id == uid) node.unlocked = true;
        sItems.clear(); if (root.contains("storageItems")) for (const auto& jItem : root["storageItems"]) sItems.push_back(JsonToItem(jItem));
        sEquip.clear(); if (root.contains("storageEquip")) for (const auto& jItem : root["storageEquip"]) sEquip.push_back(JsonToItem(jItem));
        return true;
    }
    catch (...) { return false; }
}

void DataManager::DeleteSaveData(int slot) {
    std::string filename = "save" + std::to_string(slot) + ".json";
    remove(filename.c_str());
}

SaveHeader DataManager::GetSaveHeader(int slot) {
    SaveHeader h; h.exists = false;
    std::ifstream i("save" + std::to_string(slot) + ".json");
    if (i.is_open()) { try { json j; i >> j; h.exists = true; h.floor = j.value("floor", 0); h.playerLevel = j["player"].value("level", 1); h.timestamp = "Data " + std::to_string(slot); } catch (...) {} }
    return h;
}