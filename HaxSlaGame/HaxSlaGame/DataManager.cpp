#include "DataManager.h"
#include "Player.h"
#include "AudioManager.h"
#include "raylib.h"
#include <fstream>
#include <iostream>
#include <cstdio>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

std::vector<EnemyData> DataManager::allEnemyData;
std::vector<ItemData> DataManager::itemConfigs;
std::map<std::string, std::string> DataManager::uiStrings;
std::vector<Modifier> DataManager::modifiers;
std::vector<CraftRecipe> DataManager::recipes;
std::vector<QuestData> DataManager::quests;
std::map<std::string, GameModel> DataManager::loadedModels;

Model DataManager::batModel = { 0 }; Texture2D DataManager::batTexture = { 0 }; ModelAnimation* DataManager::batAnims = nullptr; int DataManager::batAnimCount = 0; bool DataManager::isBatModelLoaded = false;
Texture2D DataManager::titleBg = { 0 };

KeyConfig DataManager::keyConfig;

static std::string FindRes(const std::string& name, const std::string& ext, const std::string& subDir) {
    std::string path1 = "resources/" + subDir + "/" + name + ext;
    if (FileExists(path1.c_str())) return path1;
    return "resources/" + name + ext;
}

static json ItemToJson(const ItemData& item) {
    return { {"id", item.id}, {"name", item.name}, {"type", item.type}, {"model", item.modelName}, {"heal", item.heal}, {"atkBonus", item.atkBonus}, {"defBonus", item.defBonus}, {"hpBonus", item.hpBonus}, {"speedBonus", item.speedBonus}, {"weaponSubtype", item.weaponSubtype}, {"count", item.count}, {"modifierId", item.modifierId}, {"dropChance", item.dropChance} };
}

static ItemData JsonToItem(const json& j) {
    ItemData d; d.id = j.value("id", -1); d.name = j.value("name", ""); d.type = j.value("type", ""); d.modelName = j.value("model", ""); d.heal = j.value("heal", 0.0f); d.atkBonus = j.value("atkBonus", 0.0f); d.defBonus = j.value("defBonus", 0.0f); d.hpBonus = j.value("hpBonus", 0.0f); d.speedBonus = j.value("speedBonus", 0.0f); d.weaponSubtype = j.value("weaponSubtype", -1); d.count = j.value("count", 1); d.modifierId = j.value("modifierId", 0); d.dropChance = j.value("dropChance", 0.0f); return d;
}

void DataManager::LoadConfig() {
    std::ifstream f("config.json");
    if (f.is_open()) {
        try {
            json j; f >> j;
            keyConfig.moveForward = j.value("moveForward", 87);
            keyConfig.moveBackward = j.value("moveBackward", 83);
            keyConfig.moveLeft = j.value("moveLeft", 65);
            keyConfig.moveRight = j.value("moveRight", 68);
            keyConfig.dash = j.value("dash", 340);
            keyConfig.smash = j.value("smash", 49);
            keyConfig.kongo = j.value("kongo", 50);
            keyConfig.zoukyou = j.value("zoukyou", 51);
            keyConfig.stealth = j.value("stealth", 52);
            keyConfig.heal = j.value("heal", 53);
            keyConfig.swapWeapon = j.value("swapWeapon", 81);

            keyConfig.padDash = j.value("padDash", 13);
            keyConfig.padSmash = j.value("padSmash", 14);
            keyConfig.padKongo = j.value("padKongo", 5);
            keyConfig.padZoukyou = j.value("padZoukyou", 6);
            keyConfig.padStealth = j.value("padStealth", 7);
            keyConfig.padHeal = j.value("padHeal", 8);
            keyConfig.padSwap = j.value("padSwap", 11);
            keyConfig.padAttack = j.value("padAttack", 15);

            if (j.contains("bgmVolume")) AudioManager::SetBGMVolume(j["bgmVolume"]);
            if (j.contains("seVolume")) AudioManager::SetSEVolume(j["seVolume"]);
        }
        catch (...) {}
    }
}

void DataManager::SaveConfig() {
    json j;
    j["moveForward"] = keyConfig.moveForward;
    j["moveBackward"] = keyConfig.moveBackward;
    j["moveLeft"] = keyConfig.moveLeft;
    j["moveRight"] = keyConfig.moveRight;
    j["dash"] = keyConfig.dash;
    j["smash"] = keyConfig.smash;
    j["kongo"] = keyConfig.kongo;
    j["zoukyou"] = keyConfig.zoukyou;
    j["stealth"] = keyConfig.stealth;
    j["heal"] = keyConfig.heal;
    j["swapWeapon"] = keyConfig.swapWeapon;

    j["padDash"] = keyConfig.padDash;
    j["padSmash"] = keyConfig.padSmash;
    j["padKongo"] = keyConfig.padKongo;
    j["padZoukyou"] = keyConfig.padZoukyou;
    j["padStealth"] = keyConfig.padStealth;
    j["padHeal"] = keyConfig.padHeal;
    j["padSwap"] = keyConfig.padSwap;
    j["padAttack"] = keyConfig.padAttack;

    j["bgmVolume"] = AudioManager::bgmVolume;
    j["seVolume"] = AudioManager::seVolume;
    std::ofstream o("config.json");
    o << j.dump(4);
}

void DataManager::LoadAllData() {
    LoadConfig();

    try {
        std::ifstream iF("items.json"); if (iF.is_open()) { json j; iF >> j; itemConfigs.clear(); for (auto& i : j) itemConfigs.push_back(JsonToItem(i)); }
        std::ifstream rF("recipes.json"); if (rF.is_open()) { json j; rF >> j; recipes.clear(); for (auto& r : j) { CraftRecipe cr; cr.resultItemId = r.value("resultId", 0); cr.cost = r.value("cost", 0); if (r.contains("materials")) for (auto& m : r["materials"]) cr.materials.push_back({ m.value("id", 0), m.value("count", 1) }); recipes.push_back(cr); } }
        std::ifstream eF("enemies.json"); if (eF.is_open()) { json j; eF >> j; allEnemyData.clear(); for (auto& i : j) { EnemyData d; d.id = i.value("id", 0); d.name = i.value("name", ""); d.modelName = i.value("model", ""); d.weaponModelName = i.value("weaponModel", ""); d.type = i.value("type", 0); d.hp = i.value("hp", 0.0f); d.speed = i.value("speed", 0.0f); d.minFloor = i.value("minFloor", 1); d.atkRange = i.value("atkRange", 2.0f); d.exp = i.value("exp", 0); d.gold = i.value("gold", 0); if (i.contains("drops")) d.drops = i["drops"].get<std::vector<int>>(); allEnemyData.push_back(d); } }
        std::ifstream uF("ui_text.json"); if (uF.is_open()) { json j; uF >> j; uiStrings.clear(); for (auto& el : j.items()) uiStrings[el.key()] = el.value().get<std::string>(); }
        std::ifstream mF("modifiers.json"); if (mF.is_open()) { json j; mF >> j; modifiers.clear(); for (auto& i : j) { Modifier m; m.id = i.value("id", 0); m.name = i.value("name", ""); m.atk = i.value("atk", 0.0f); m.def = i.value("def", 0.0f); m.hp = i.value("hp", 0.0f); m.spd = i.value("spd", 0.0f); modifiers.push_back(m); } }
        std::ifstream qF("quests.json"); if (qF.is_open()) { json j; qF >> j; quests.clear(); for (auto& q : j) { QuestData qd; qd.id = q.value("id", 0); qd.title = q.value("title", ""); qd.description = q.value("description", ""); std::string t = q.value("type", "HUNT"); qd.type = (t == "GATHER") ? QUEST_GATHER : QUEST_HUNT; qd.targetId = q.value("targetId", 0); qd.targetCount = q.value("targetCount", 0); qd.rewardGold = q.value("rewardGold", 0); qd.rewardItemId = q.value("rewardItemId", -1); qd.rewardItemCount = q.value("rewardItemCount", 0); quests.push_back(qd); } }
    }
    catch (const std::exception& e) { std::cerr << "JSON Load Error: " << e.what() << std::endl; }

    for (const auto& enemy : allEnemyData) {
        std::string name = enemy.modelName;
        if (!name.empty() && loadedModels.count(name) == 0) {
            std::string iqmPath = FindRes(name, ".iqm", "IQM"); std::string texPath = FindRes(name + "_Albedo", ".png", "Texture"); if (!FileExists(texPath.c_str())) texPath = FindRes("Albedo", ".png", "Texture");
            if (FileExists(iqmPath.c_str()) && FileExists(texPath.c_str())) {
                GameModel gm; gm.model = LoadModel(iqmPath.c_str()); gm.texture = LoadTexture(texPath.c_str());
                for (int m = 0; m < gm.model.materialCount; m++) { SetMaterialTexture(&gm.model.materials[m], MATERIAL_MAP_DIFFUSE, gm.texture); }
                gm.anims = LoadModelAnimations(iqmPath.c_str(), &gm.animCount); gm.loaded = true; loadedModels[name] = gm;
            }
        }
        std::string wName = enemy.weaponModelName;
        if (!wName.empty() && loadedModels.count(wName) == 0) {
            std::string wIqmPath = FindRes(wName, ".iqm", "IQM"); if (!FileExists(wIqmPath.c_str())) wIqmPath = FindRes(wName, ".obj", "OBJ"); std::string wTexPath = FindRes(wName + "_Albedo", ".png", "Texture"); if (!FileExists(wTexPath.c_str())) wTexPath = FindRes("Albedo", ".png", "Texture");
            if (FileExists(wIqmPath.c_str())) {
                GameModel wGm; wGm.model = LoadModel(wIqmPath.c_str());
                if (FileExists(wTexPath.c_str())) {
                    wGm.texture = LoadTexture(wTexPath.c_str());
                    for (int m = 0; m < wGm.model.materialCount; m++) { SetMaterialTexture(&wGm.model.materials[m], MATERIAL_MAP_DIFFUSE, wGm.texture); }
                }
                wGm.anims = LoadModelAnimations(wIqmPath.c_str(), &wGm.animCount); wGm.loaded = true; loadedModels[wName] = wGm;
            }
        }
    }

    std::vector<std::string> envModels = {
        "Obj_Storage", "Obj_Reforge", "Obj_Craft", "Obj_Portal", "Obj_StairsDown", "Obj_StairsUp", "Obj_Heal", "Obj_QuestBoard", "Player",
        "Wpn_Sword", "Wpn_Spear", "Wpn_Axe", "Wpn_Wand",
        "Wpn_Sword_Legend", "Wpn_Spear_Legend", "Wpn_Axe_Legend", "Wpn_Wand_Legend"
    };
    for (const auto& mName : envModels) {
        std::string objPath = FindRes(mName, ".obj", "OBJ"); std::string iqmPath = FindRes(mName, ".iqm", "IQM"); std::string texPath = FindRes(mName + "_Albedo", ".png", "Texture");
        std::string usePath = ""; if (FileExists(iqmPath.c_str())) usePath = iqmPath; else if (FileExists(objPath.c_str())) usePath = objPath;
        if (!usePath.empty()) {
            GameModel gm; gm.model = LoadModel(usePath.c_str());
            if (!FileExists(texPath.c_str())) texPath = FindRes("Albedo", ".png", "Texture");
            if (FileExists(texPath.c_str())) {
                gm.texture = LoadTexture(texPath.c_str());
                for (int m = 0; m < gm.model.materialCount; m++) { SetMaterialTexture(&gm.model.materials[m], MATERIAL_MAP_DIFFUSE, gm.texture); }
            }
            if (usePath == iqmPath) gm.anims = LoadModelAnimations(usePath.c_str(), &gm.animCount); else { gm.anims = nullptr; gm.animCount = 0; }
            gm.loaded = true; loadedModels[mName] = gm;
        }
    }

    for (auto& cfg : itemConfigs) {
        if (cfg.type == "EQUIP") {
            std::string wKey = cfg.modelName.empty() ? ("Wpn_" + std::to_string(cfg.id)) : cfg.modelName;

            if (loadedModels.count(wKey) == 0) {
                std::string wObj = FindRes(wKey, ".obj", "OBJ"); if (!FileExists(wObj.c_str())) wObj = FindRes(wKey, ".iqm", "IQM");
                if (FileExists(wObj.c_str())) {
                    GameModel gm; gm.model = LoadModel(wObj.c_str()); std::string wTex = FindRes(wKey + "_Albedo", ".png", "Texture"); if (!FileExists(wTex.c_str())) wTex = FindRes("Albedo", ".png", "Texture");
                    if (FileExists(wTex.c_str())) {
                        gm.texture = LoadTexture(wTex.c_str());
                        for (int m = 0; m < gm.model.materialCount; m++) { SetMaterialTexture(&gm.model.materials[m], MATERIAL_MAP_DIFFUSE, gm.texture); }
                    }
                    gm.anims = nullptr; gm.animCount = 0; gm.loaded = true; loadedModels[wKey] = gm;
                }
            }
        }
    }

    if (loadedModels.count("Bat")) { batModel = loadedModels["Bat"].model; batTexture = loadedModels["Bat"].texture; batAnims = loadedModels["Bat"].anims; batAnimCount = loadedModels["Bat"].animCount; isBatModelLoaded = true; }
    std::string titleBgPath = FindRes("title_bg", ".png", "Texture"); if (FileExists(titleBgPath.c_str())) { titleBg = LoadTexture(titleBgPath.c_str()); }
}

void DataManager::UnloadAllData() {
    for (auto& pair : loadedModels) { if (pair.second.loaded) { UnloadTexture(pair.second.texture); if (pair.second.anims != nullptr) UnloadModelAnimations(pair.second.anims, pair.second.animCount); UnloadModel(pair.second.model); } }
    loadedModels.clear(); isBatModelLoaded = false; if (titleBg.id != 0) { UnloadTexture(titleBg); }
}

EnemyData DataManager::GetRandomEnemyForFloor(int floor, int dungeonId) {
    std::vector<EnemyData> c;
    for (auto& e : allEnemyData) {
        if (e.id == 15 || e.id == 16 || e.id == 17 || e.id == 18 || e.id == 20 || e.id == 21 || e.id == 22) continue;
        bool ok = false; if (dungeonId == 0) { if (e.minFloor <= 10) ok = true; }
        else if (dungeonId == 1) { if (e.minFloor >= 6 && e.minFloor <= 15) ok = true; }
        else { if (e.minFloor >= 10) ok = true; }
        if (ok) c.push_back(e);
    }
    if (c.empty()) return allEnemyData.empty() ? EnemyData{ 0, "Slime", "", "", 0, 30, 0.05f } : allEnemyData[0];
    return c[GetRandomValue(0, (int)c.size() - 1)];
}

EnemyData DataManager::GetBossEnemy(int floor, int dungeonId) {
    if (allEnemyData.empty()) return { 0, "Boss", "", "", 0, 1000, 0.1f };
    std::vector<EnemyData> bosses;
    if (dungeonId == 0) { for (auto& e : allEnemyData) if (e.id == 7 || e.id == 8) bosses.push_back(e); if (floor == 30) { for (auto& e : allEnemyData) if (e.id == 15) return e; } }
    else if (dungeonId == 1) { for (auto& e : allEnemyData) if (e.id == 15 || e.id == 16 || e.id == 17) bosses.push_back(e); if (floor == 50) { for (auto& e : allEnemyData) if (e.id == 17) return e; } }
    else { for (auto& e : allEnemyData) if (e.id == 17 || e.id == 18 || e.id == 20 || e.id == 21) bosses.push_back(e); if (floor == 100) { for (auto& e : allEnemyData) if (e.id == 22) return e; } }
    if (!bosses.empty()) return bosses[GetRandomValue(0, (int)bosses.size() - 1)]; return allEnemyData[0];
}

ItemData DataManager::GetItemConfigCopy(int id) { for (auto& cfg : itemConfigs) if (cfg.id == id) return cfg; ItemData empty; empty.id = -1; return empty; }
QuestData DataManager::GetQuestData(int id) { for (auto& q : quests) if (q.id == id) return q; QuestData empty; empty.id = -1; return empty; }
Modifier DataManager::GetModifier(int id) { for (const auto& mod : modifiers) if (mod.id == id) return mod; for (const auto& mod : modifiers) if (mod.id == 0) return mod; return { 0, "", 0,0,0,0 }; }
int DataManager::GetRandomModifierId() { if (modifiers.size() <= 1) return 0; return modifiers[GetRandomValue(1, (int)modifiers.size() - 1)].id; }

void DataManager::SaveGame(int slot, Player* p, int currentFloor, int currentDungeon, int unlockedDungeon, const std::vector<int>& maxFloors, const std::vector<ItemData>& sItems, const std::vector<ItemData>& sEquip, bool isPortfolio) {
    json root; root["floor"] = currentFloor; root["currentDungeonId"] = currentDungeon; root["unlockedDungeonId"] = unlockedDungeon;
    root["maxFloors"] = json::array(); for (int f : maxFloors) root["maxFloors"].push_back(f);
    root["isPortfolioMode"] = isPortfolio;
    root["player"] = { {"level", p->level}, {"exp", p->exp}, {"expToNext", p->expToNext}, {"hp", p->hp}, {"maxHp", p->maxHp}, {"attackPower", p->attackPower}, {"defense", p->defense}, {"skillPoints", p->skillPoints}, {"gold", p->gold} };
    root["inventoryItems"] = json::array(); for (const auto& item : p->inventoryItems) root["inventoryItems"].push_back(ItemToJson(item));
    root["inventoryEquip"] = json::array(); for (const auto& item : p->inventoryEquip) root["inventoryEquip"].push_back(ItemToJson(item));
    root["equipped0"] = ItemToJson(p->equippedData[0]); root["equipped1"] = ItemToJson(p->equippedData[1]);
    root["armor"] = json::array(); for (int i = 0; i < 5; i++) root["armor"].push_back(ItemToJson(p->equippedArmor[i]));
    root["unlockedSkills"] = json::array(); for (const auto& node : p->skillTree) if (node.unlocked) root["unlockedSkills"].push_back(node.id);
    root["storageItems"] = json::array(); for (const auto& item : sItems) root["storageItems"].push_back(ItemToJson(item));
    root["storageEquip"] = json::array(); for (const auto& item : sEquip) root["storageEquip"].push_back(ItemToJson(item));
    root["activeQuests"] = json::array(); for (const auto& q : p->activeQuests) { root["activeQuests"].push_back({ {"id", q.questId}, {"count", q.currentCount}, {"completed", q.isCompleted} }); }
    root["clearedQuests"] = json::array(); for (int cid : p->clearedQuests) { root["clearedQuests"].push_back(cid); }
    std::ofstream o("save" + std::to_string(slot) + ".json"); o << root.dump(4);
}

bool DataManager::LoadGame(int slot, Player* p, int& currentFloor, int& currentDungeon, int& unlockedDungeon, std::vector<int>& maxFloors, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip, bool& isPortfolio) {
    std::ifstream i("save" + std::to_string(slot) + ".json"); if (!i.is_open()) return false;
    try {
        json root; i >> root; currentFloor = root.value("floor", 0); currentDungeon = root.value("currentDungeonId", 0); unlockedDungeon = root.value("unlockedDungeonId", 0);
        maxFloors = { 0, 0, 0 }; if (root.contains("maxFloors")) { int idx = 0; for (auto& f : root["maxFloors"]) { if (idx < 3) maxFloors[idx] = f.get<int>(); idx++; } }
        else { maxFloors[0] = root.value("maxReachedFloor", 0); }
        isPortfolio = root.value("isPortfolioMode", false);
        auto& jp = root["player"]; p->level = jp.value("level", 1); p->exp = jp.value("exp", 0); p->expToNext = jp.value("expToNext", 100); p->hp = jp.value("hp", 100.0f); p->maxHp = jp.value("maxHp", 100.0f); p->attackPower = jp.value("attackPower", 10.0f); p->defense = jp.value("defense", 0.0f); p->skillPoints = jp.value("skillPoints", 0); p->gold = jp.value("gold", 0);
        p->inventoryItems.clear(); for (const auto& jItem : root["inventoryItems"]) p->inventoryItems.push_back(JsonToItem(jItem));
        p->inventoryEquip.clear(); for (const auto& jItem : root["inventoryEquip"]) p->inventoryEquip.push_back(JsonToItem(jItem));
        p->equippedData[0] = JsonToItem(root["equipped0"]); p->equippedWeapons[0] = (p->equippedData[0].id == -1) ? NONE : (WeaponType)p->equippedData[0].weaponSubtype;
        p->equippedData[1] = JsonToItem(root["equipped1"]); p->equippedWeapons[1] = (p->equippedData[1].id == -1) ? NONE : (WeaponType)p->equippedData[1].weaponSubtype;
        p->currentWeapon = p->equippedWeapons[p->activeSlot];
        if (root.contains("armor")) { int idx = 0; for (const auto& jItem : root["armor"]) { if (idx < 5) p->equippedArmor[idx] = JsonToItem(jItem); idx++; } }
        std::vector<int> unlockedIds = root.value("unlockedSkills", std::vector<int>()); for (int uid : unlockedIds) for (auto& node : p->skillTree) if (node.id == uid) node.unlocked = true;
        sItems.clear(); if (root.contains("storageItems")) for (const auto& jItem : root["storageItems"]) sItems.push_back(JsonToItem(jItem));
        sEquip.clear(); if (root.contains("storageEquip")) for (const auto& jItem : root["storageEquip"]) sEquip.push_back(JsonToItem(jItem));
        p->activeQuests.clear(); if (root.contains("activeQuests")) { for (const auto& jq : root["activeQuests"]) { PlayerQuest pq; pq.questId = jq.value("id", 0); pq.currentCount = jq.value("count", 0); pq.isCompleted = jq.value("completed", false); p->activeQuests.push_back(pq); } }
        p->clearedQuests.clear(); if (root.contains("clearedQuests")) { for (const auto& jc : root["clearedQuests"]) { p->clearedQuests.push_back(jc.get<int>()); } }
        return true;
    }
    catch (...) { return false; }
}

SaveHeader DataManager::GetSaveHeader(int slot) { SaveHeader h; h.exists = false; std::ifstream i("save" + std::to_string(slot) + ".json"); if (i.is_open()) { try { json j; i >> j; h.exists = true; h.floor = j.value("floor", 0); h.playerLevel = j["player"].value("level", 1); h.timestamp = "Data " + std::to_string(slot); h.isPortfolioMode = j.value("isPortfolioMode", false); h.unlockedDungeonId = j.value("unlockedDungeonId", 0); } catch (...) {} } return h; }
void DataManager::DeleteSave(int slot) { std::string filename = "save" + std::to_string(slot) + ".json"; std::remove(filename.c_str()); }