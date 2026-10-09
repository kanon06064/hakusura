#include "DebugMenu.h"
#include "Game.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
#include "EffectManager.h"
#include "imgui.h"
#include <string>

// =============================================================================
// デバッグメニュー用：完全英数字変換ヘルパー（文字化け完全防止）
// =============================================================================

static std::string GetEngModifierName(int modId) {
    switch (modId) {
    case 1: return "Broken";
    case 2: return "Crude";
    case 3: return "Dull";
    case 4: return "Sharp";
    case 5: return "Strong";
    case 6: return "Master";
    case 7: return "Legendary";
    case 10: return "Hard";
    case 11: return "Guarded";
    case 12: return "Swift";
    case 13: return "Heavy";
    case 14: return "Godspeed";
    case 15: return "Arcana";
    default: return "";
    }
}

static std::string GetEngElementName(int elem) {
    switch (elem) {
    case 1: return "Hellfire";
    case 2: return "Rot";
    case 3: return "Soul";
    case 4: return "Abyss";
    default: return "None";
    }
}

static std::string GetEngItemBaseName(const ItemData& item) {
    if (item.id == -1) return "Empty";
    if (!item.modelName.empty()) return item.modelName;

    if (item.type == "EQUIP") {
        const char* wTypes[] = { "Sword", "Spear", "Axe", "Wand" };
        if (item.weaponSubtype >= 0 && item.weaponSubtype <= 3)
            return std::string(wTypes[item.weaponSubtype]) + "_" + std::to_string(item.id);
        return "Weapon_" + std::to_string(item.id);
    }
    else if (item.type == "ARMOR") {
        const char* aTypes[] = { "Helm", "Armor", "Gauntlet", "Legs", "Boots" };
        if (item.weaponSubtype >= 0 && item.weaponSubtype <= 4)
            return std::string(aTypes[item.weaponSubtype]) + "_" + std::to_string(item.id);
        return "Armor_" + std::to_string(item.id);
    }
    else if (item.type == "CONSUMABLE") {
        return "Consumable_" + std::to_string(item.id);
    }
    return "Material_" + std::to_string(item.id);
}

static std::string GetEngFullItemName(const ItemData& item) {
    if (item.id == -1) return "(Empty Slot)";
    std::string fullName = "";

    std::string modStr = GetEngModifierName(item.modifierId);
    if (!modStr.empty()) fullName += "[" + modStr + "] ";

    if (item.element != 0) fullName += "<" + GetEngElementName(item.element) + "> ";

    fullName += GetEngItemBaseName(item);
    return fullName;
}

// =============================================================================
// デバッグメニュー本体
// =============================================================================
void DebugMenu::Draw(Game* game) {
    if (!game) return;

    ImGui::SetNextWindowSize(ImVec2(660, 560), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Developer Tools (F1 to toggle)")) {
        ImGui::Text("FPS: %d  |  State: %d", GetFPS(), (int)game->state);
        ImGui::Separator();

        if (ImGui::BeginTabBar("DebugTabs")) {

            // =================================================================
            // タブ1: プレイヤー・ステータス操作
            // =================================================================
            if (ImGui::BeginTabItem("Player / Stats")) {
                Player* p = game->player;
                if (p) {
                    ImGui::Text("Level: %d  |  EXP: %d/%d  |  SP: %d", p->level, p->exp, p->expToNext, p->skillPoints);
                    ImGui::Text("HP: %.0f / %.0f  |  ATK: %.1f  |  DEF: %.1f", p->hp, p->maxHp, p->attackPower, p->defense);
                    ImGui::Text("Gold: %d G", p->gold);
                    ImGui::Separator();

                    if (ImGui::Button("Heal Full HP", ImVec2(120, 30))) {
                        p->hp = p->maxHp;
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Level Control:");
                    if (ImGui::Button("Lv +1")) { p->level += 1;  p->skillPoints += 3; p->RecalculateStats(); p->hp = p->maxHp; }
                    ImGui::SameLine();
                    if (ImGui::Button("Lv +10")) { p->level += 10; p->skillPoints += 30; p->RecalculateStats(); p->hp = p->maxHp; }
                    ImGui::SameLine();
                    if (ImGui::Button("Lv +99")) { p->level += 99; p->skillPoints += 297; p->RecalculateStats(); p->hp = p->maxHp; }
                    ImGui::SameLine();
                    if (ImGui::Button("Lv -1 (Level Down)")) {
                        if (p->level > 1) {
                            p->level -= 1;
                            if (p->skillPoints >= 3) p->skillPoints -= 3;
                            p->RecalculateStats();
                            p->hp = p->maxHp;
                        }
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1, 1), "Skill Points (SP):");
                    if (ImGui::Button("SP +1")) { p->skillPoints += 1; }
                    ImGui::SameLine();
                    if (ImGui::Button("SP +10")) { p->skillPoints += 10; }
                    ImGui::SameLine();
                    if (ImGui::Button("SP +100")) { p->skillPoints += 100; }
                    ImGui::SameLine();
                    if (ImGui::Button("Unlock All Skills")) {
                        for (auto& node : p->skillTree) node.unlocked = true;
                        p->RecalculateStats();
                    }

                    ImGui::Spacing();
                    ImGui::TextColored(ImVec4(1, 1, 0, 1), "Gold Control:");
                    if (ImGui::Button("Gold +1,000")) { p->gold += 1000; }
                    ImGui::SameLine();
                    if (ImGui::Button("Gold +100,000")) { p->gold += 100000; }
                }
                ImGui::EndTabItem();
            }

            // =================================================================
            // タブ2: ダンジョン進行・フロア操作
            // =================================================================
            if (ImGui::BeginTabItem("Dungeon / Floor")) {
                ImGui::Text("Current Dungeon: %d  |  Current Floor: %d", game->currentDungeonId, game->floor);
                ImGui::Text("Boss Defeated: %s", game->bossDefeated ? "YES" : "NO");
                ImGui::Separator();

                if (ImGui::Button("Next Floor >>", ImVec2(150, 35))) {
                    game->NextFloor();
                }
                ImGui::SameLine();
                if (ImGui::Button("Return Home", ImVec2(150, 35))) {
                    game->ReturnHome();
                }

                ImGui::Spacing();
                ImGui::Text("Warp to Floor:");
                for (int f = 1; f <= 50; f += 5) {
                    if (ImGui::Button(TextFormat("B%dF", f), ImVec2(50, 25))) {
                        game->WarpToFloor(game->currentDungeonId, f);
                    }
                    if (f % 25 != 0) ImGui::SameLine();
                }
                ImGui::EndTabItem();
            }

            // =================================================================
            // タブ3: 戦闘・敵の操作（完全英名表示）
            // =================================================================
            if (ImGui::BeginTabItem("Enemies / Combat")) {
                ImGui::Text("Enemies Count: %d", (int)game->enemies.size());

                if (ImGui::Button("Kill All Enemies (Instant)", ImVec2(200, 35))) {
                    for (auto& e : game->enemies) e.hp = 0;
                }
                ImGui::SameLine();
                if (ImGui::Button("Spawn +5 Enemies")) {
                    game->SpawnEnemies((int)game->enemies.size() + 5);
                }

                ImGui::Separator();
                ImGui::Text("Enemy List in current floor:");
                ImGui::BeginChild("EnemyListChild", ImVec2(0, 200), true);
                for (size_t i = 0; i < game->enemies.size(); i++) {
                    auto& e = game->enemies[i];
                    std::string eName = e.data.modelName;
                    if (eName.empty()) eName = "Enemy_" + std::to_string(e.data.id);

                    std::string eElem = GetEngElementName(e.data.element);

                    ImGui::Text("[%d] %s (Lv.%d) - HP: %.0f/%.0f [%s]",
                        (int)i, eName.c_str(), e.level, e.hp, e.maxHp, eElem.c_str());

                    ImGui::SameLine();
                    if (ImGui::Button(TextFormat("Kill##%d", (int)i))) { e.hp = 0; }
                    ImGui::SameLine();
                    if (ImGui::Button(TextFormat("Burn##%d", (int)i))) { e.status.ApplyStatus(ELEM_HELLFIRE, 4.0f, 30.0f); }
                    ImGui::SameLine();
                    if (ImGui::Button(TextFormat("Freeze##%d", (int)i))) { e.status.ApplyStatus(ELEM_SOUL, 4.0f, 0.0f); }
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // =================================================================
            // タブ4: アイテム生成（完全英名表示）
            // =================================================================
            if (ImGui::BeginTabItem("Item Spawner")) {
                static int selectElem = 0;
                static int selectMod = 0;
                const char* elemNames[] = { "None", "Hellfire", "Rot", "Soul", "Abyss" };
                const char* modOptions[] = {
                    "None(0)", "Broken(1)", "Crude(2)", "Dull(3)", "Sharp(4)", "Strong(5)",
                    "Master(6)", "Legendary(7)", "Hard(10)", "Guarded(11)", "Swift(12)",
                    "Heavy(13)", "Godspeed(14)", "Arcana(15)"
                };
                static int modValues[] = { 0, 1, 2, 3, 4, 5, 6, 7, 10, 11, 12, 13, 14, 15 };
                static int modComboIdx = 0;

                ImGui::TextColored(ImVec4(1, 1, 0, 1), "Spawn Config for Equipments:");
                ImGui::Combo("Element", &selectElem, elemNames, 5);
                if (ImGui::Combo("Modifier", &modComboIdx, modOptions, 14)) {
                    selectMod = modValues[modComboIdx];
                }

                ImGui::Separator();
                ImGui::BeginChild("ItemSpawnList", ImVec2(0, 300), true);
                for (auto& cfg : DataManager::itemConfigs) {
                    ImGui::PushID(cfg.id);

                    // 英語のアイテムベース名を表示（文字化け完全防止）
                    std::string engItemName = GetEngItemBaseName(cfg);
                    ImGui::Text("[%s] ID:%d %s", cfg.type.c_str(), cfg.id, engItemName.c_str());

                    ImGui::SameLine(ImGui::GetWindowWidth() - 70);
                    if (ImGui::Button("Get")) {
                        if (game->player) {
                            ItemData item = cfg;
                            if (item.type == "EQUIP" || item.type == "ARMOR") {
                                item.modifierId = selectMod;
                                item.element = selectElem;
                            }
                            game->player->AddToInventory(item);
                        }
                    }
                    ImGui::PopID();
                }
                ImGui::EndChild();
                ImGui::EndTabItem();
            }

            // =================================================================
            // タブ5: 鍛冶・エンチャント・リフォージ（完全英名表示）
            // =================================================================
            if (ImGui::BeginTabItem("Reforge & Enchant")) {
                Player* p = game->player;
                if (p) {
                    ItemData& curWpn = p->equippedData[p->activeSlot];

                    // 英語で現在の武器名・属性・モディファイアを表示
                    std::string curWpnName = GetEngFullItemName(curWpn);
                    ImGui::TextColored(ImVec4(1, 0.8f, 0, 1), "Active Weapon Slot: %d", p->activeSlot + 1);
                    ImGui::Text("Name: %s", curWpnName.c_str());

                    if (curWpn.id != -1) {
                        std::string curMod = GetEngModifierName(curWpn.modifierId);
                        if (curMod.empty()) curMod = "None";
                        ImGui::Text("Current Modifier: %s (ID: %d)", curMod.c_str(), curWpn.modifierId);
                        ImGui::Text("Current Element:  %s", GetEngElementName(curWpn.element).c_str());

                        ImGui::Spacing();
                        ImGui::Separator();

                        // 1. ワンクリックモディファイアリフォージ
                        if (ImGui::Button("Random Reforge (Modifier)", ImVec2(220, 32))) {
                            curWpn.modifierId = DataManager::GetRandomModifierId();
                        }

                        // 2. ワンクリック属性エンチャント
                        ImGui::Spacing();
                        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1, 1), "Instant Element Enchant:");
                        if (ImGui::Button("Enchant: Hellfire")) { curWpn.element = ELEM_HELLFIRE; }
                        ImGui::SameLine();
                        if (ImGui::Button("Enchant: Rot")) { curWpn.element = ELEM_ROT; }

                        if (ImGui::Button("Enchant: Soul")) { curWpn.element = ELEM_SOUL; }
                        ImGui::SameLine();
                        if (ImGui::Button("Enchant: Abyss")) { curWpn.element = ELEM_ABYSS; }
                        ImGui::SameLine();
                        if (ImGui::Button("Clear Element")) { curWpn.element = ELEM_NONE; }
                    }
                    else {
                        ImGui::TextColored(ImVec4(1, 0.2f, 0.2f, 1), "No Weapon Equipped in active slot!");
                    }
                }
                ImGui::EndTabItem();
            }

            // =================================================================
            // タブ6: 武器オフセット微調整 (Tweaker)
            // =================================================================
            if (ImGui::BeginTabItem("Weapon Tweaker")) {
                ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Adjust in-hand weapon transform:");
                ImGui::DragFloat("Scale (Size)", &Player::customWeaponScale, 0.01f, 0.01f, 100.0f);
                ImGui::DragFloat3("Position", &Player::customWeaponOffsetPos.x, 0.01f);
                ImGui::DragFloat3("Rotation", &Player::customWeaponOffsetRot.x, 1.0f);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();
}