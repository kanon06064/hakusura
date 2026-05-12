#include "Game.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "raymath.h"
#include "rlgl.h"

static std::string T(const std::string& key, const std::string& def) {
    if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
    return def;
}

void Game::DrawDebugRoom() {
    ClearBackground(BLACK);
    BeginMode3D(camera);
    DrawGrid(100, 1.0f);
    for (auto& e : enemies) e.Draw(true, camera, font, camera.position);
    EndMode3D();

    DrawTextEx(font, T("DEBUG_ROOM_TITLE", "Debug Room (TAB/ESC to return)").c_str(), { 20, 20 }, 20, 1, WHITE);
    DrawTextEx(font, T("DEBUG_CAM_INFO", "WASD + Mouse to move camera").c_str(), { 20, 50 }, 20, 1, LIGHTGRAY);

    for (auto& e : enemies) {
        Vector2 screenPos = GetWorldToScreen(e.position, camera);
        if (screenPos.x > 0 && screenPos.y > 0) {
            std::string stateName = "Idle";
            if (e.isDying) stateName = "Die";
            else if (e.state == STATE_CHASE) stateName = "Run";
            else if (e.state == STATE_ATTACK) stateName = "Attack";

            DrawTextEx(font, e.data.name.c_str(), { screenPos.x, screenPos.y - 20 }, 10, 1, WHITE);
            DrawTextEx(font, stateName.c_str(), { screenPos.x, screenPos.y }, 10, 1, YELLOW);
        }
    }
}

void Game::Draw() {
    BeginDrawing();
    if (state == STATE_TITLE) {
        int slot = UI::DrawTitleScreen(font);
        if (slot > 0) {
            if (slot == 999) StartDebugRoom();
            else if (slot == 888) StartPortfolioMode();
            else {
                SaveHeader h = DataManager::GetSaveHeader(slot);
                if (h.exists) LoadAndStart(slot);
                else NewGameAndStart(slot);
            }
        }
    }
    else if (state == STATE_DEBUG_ROOM) { DrawDebugRoom(); }
    else if (state == STATE_GAMEOVER) {
        ClearBackground(BLACK);
        DrawTextEx(font, T("DEATH_TITLE", "YOU DIED").c_str(), { (float)screenWidth / 2 - 100, (float)screenHeight / 2 - 50 }, 60, 2, RED);
        DrawTextEx(font, T("DEATH_DESC", "Lost items and returning home...").c_str(), { (float)screenWidth / 2 - 180, (float)screenHeight / 2 + 30 }, 24, 1, WHITE);
        DrawTextEx(font, T("CLICK_TO_CONT", "Click to Continue").c_str(), { (float)screenWidth / 2 - 80, (float)screenHeight / 2 + 80 }, 20, 1, LIGHTGRAY);
    }
    else if (state == STATE_GAMECLEAR) {
        ClearBackground(RAYWHITE);
        DrawTextEx(font, T("CLEAR_TITLE", "GAME CLEAR!!").c_str(), { (float)screenWidth / 2 - 150, (float)screenHeight / 2 - 60 }, 60, 2, GOLD);
        DrawTextEx(font, T("CLEAR_DESC", "Demon Lord Defeated!").c_str(), { (float)screenWidth / 2 - 120, (float)screenHeight / 2 + 20 }, 24, 1, BLACK);
        DrawTextEx(font, T("CLICK_TO_RETURN", "Click to Return Home").c_str(), { (float)screenWidth / 2 - 100, (float)screenHeight / 2 + 70 }, 20, 1, DARKGRAY);
    }
    else {
        ClearBackground(BLACK); BeginMode3D(camera);
        dungeon.Draw();

        bool hideStairs = false;
        if (!isPortfolioMode && floor > 0 && floor % 10 == 0 && !bossDefeated) hideStairs = true;
        if (isPortfolioMode && (floor == 2 || floor == 3) && !bossDefeated) hideStairs = true;

        if (hideStairs && dungeon.stairsDownPos.x != -999) {
            DrawCube(dungeon.stairsDownPos, 2.1f, 2.0f, 2.1f, BLACK);
        }

        fxManager.Draw();

        for (auto& item : droppedItems) {
            if (!debugMode && !dungeon.IsDiscovered(item.pos.x, item.pos.z)) continue;

            Color rarityCol = Player::GetItemRarityColor(item.data);

            rlPushMatrix();
            rlTranslatef(item.pos.x, item.pos.y, item.pos.z);
            rlRotatef(item.rotation, 0, 1, 0);

            DrawCube({ 0,0,0 }, 0.5f, 0.4f, 0.5f, rarityCol);
            DrawCubeWires({ 0,0,0 }, 0.5f, 0.4f, 0.5f, WHITE);
            rlPopMatrix();

            if (item.data.type == "EQUIP" || item.data.type == "ARMOR") {
                Color pillarCol = rarityCol;
                float alpha = (sinf(GetTime() * 5.0f) * 0.5f + 0.5f) * 0.5f + 0.1f;
                pillarCol.a = (unsigned char)(255 * alpha);
                DrawCylinder(item.pos, 0.15f, 0.15f, 5.0f, 8, pillarCol);
            }
        }

        for (auto& e : enemies) if (debugMode || dungeon.IsDiscovered(e.position.x, e.position.z)) e.Draw(debugMode, camera, font, player->position);

        player->Draw(debugMode);
        EndMode3D();

        fxManager.Draw2D(font, camera);

        int displayFloor = isPortfolioMode ? (floor + 1000) : floor;

        UI::DrawHUD(*player, enemies, dungeon, camera, displayFloor, currentDungeonId, debugMode, font);
        UI::DrawLogs(logs, *player, camera, font);
        UI::DrawNearbyItems(*player, droppedItems, dungeon, camera, font);

        UI::UpdateSystemLogs(GetFrameTime());
        UI::DrawSystemLogs(font);

        if (showMenu) {
            int menuEvent = UI::DrawMenu(*player, dungeon, currentTab, font);

            if (menuEvent == 1) {
                SaveCurrentSlot();
                UI::AddSystemLog(T("LOG_GAME_SAVED", "GAME SAVED!"), GREEN);
            }
            else if (menuEvent == 2) {
                state = STATE_TITLE;
                isPortfolioMode = false;
                showMenu = false;
                AudioManager::PlayBGM(BGM_TITLE);
            }
        }

        if (showStorage) UI::DrawStorage(*player, font, showStorage, storageItems, storageEquip);
        if (showReforgeMenu) UI::DrawReforgeMenu(*player, font, showReforgeMenu);
        if (showWarpMenu) UI::DrawWarpMenu(this, unlockedDungeonId, maxFloors, font, showWarpMenu);
        if (showCraftMenu) UI::DrawCraftingMenu(*player, font, showCraftMenu);
        if (showQuestMenu) UI::DrawQuestMenu(*player, font, showQuestMenu);

        if (showPrompt) {
            int maxF = (currentDungeonId == 0) ? 30 : (currentDungeonId == 1) ? 50 : 100;
            const char* m = "UNKNOWN";

            if (state == STATE_HOME && hoveredEntranceIndex != -1) m = "ENTER_DUNGEON";
            else if (state == STATE_DUNGEON) {
                if (floor == maxF) m = "RETURN_HOME";
                else if (Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) m = "GO_DEEPER";
                else if (Vector3Distance(player->position, dungeon.stairsUpPos) < 2.0f) m = "RETURN_HOME";
                else if (dungeon.portalPos.x != -999 && Vector3Distance(player->position, dungeon.portalPos) < 2.0f) m = "RETURN_HOME";
            }

            int res = UI::DrawPrompt(m, screenWidth, screenHeight, font);
            if (res == 1) {
                if (state == STATE_HOME && hoveredEntranceIndex != -1) {
                    currentDungeonId = hoveredEntranceIndex;
                    floor = 0;
                    NextFloor();
                }
                else if (state == STATE_DUNGEON && floor > 0 && floor != maxF && Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) {
                    NextFloor();
                }
                else {
                    ReturnHome();
                }
                AudioManager::PlaySE(SE_STAIRS);
                showPrompt = false;
                sceneTimer = 2.0f;
            }
            else if (res == 2) { showPrompt = false; sceneTimer = 1.0f; }
        }
    }

    rlImGuiBegin();
    if (debugMode && state != STATE_TITLE) {
        ImGui::Begin("Developer Tools (F1 to toggle)");
        ImGui::Text("FPS: %d", GetFPS());
        ImGui::Separator();

        // ★追加: デバッグ画面で現在読み込まれているモデルの状態を詳細表示
        if (DataManager::loadedModels.count("Player") > 0) {
            GameModel& pm = DataManager::loadedModels["Player"];
            ImGui::TextColored(ImVec4(1, 1, 0, 1), "Player Model Status:");
            ImGui::Text("Format: %s", pm.animCount > 0 ? "IQM (Animated)" : "OBJ (Static Mesh)");
            ImGui::Text("Bones: %d", pm.model.boneCount);
            ImGui::Text("Animations: %d", pm.animCount);
            ImGui::Separator();
        }

        if (player) {
            ImGui::Text("Player POS: (%.1f, %.1f, %.1f)", player->position.x, player->position.y, player->position.z);
            ImGui::Text("Dungeon: %d  Floor: %d", currentDungeonId, floor);
            ImGui::Text("HP: %.0f / %.0f", player->hp, player->maxHp);
            ImGui::Text("Level: %d  EXP: %d", player->level, player->exp);
            ImGui::Separator();
        }
        ImGui::Text("Enemies Count: %d", (int)enemies.size());
        if (ImGui::BeginChild("EnemyList", ImVec2(0, 150), true)) {
            for (size_t i = 0; i < enemies.size(); i++) {
                ImGui::Text("[%d] %s  HP: %.1f/%.1f", (int)i, enemies[i].data.modelName.c_str(), enemies[i].hp, enemies[i].maxHp);
            }
            ImGui::EndChild();
        }
        if (ImGui::Button("Heal Player")) { if (player) player->hp = player->maxHp; }
        ImGui::SameLine();
        if (ImGui::Button("Kill All Enemies")) { for (auto& e : enemies) e.hp = 0; }

        ImGui::Separator();
        if (ImGui::Button("Level +99")) {
            if (player) {
                player->level += 99;
                player->skillPoints += 99 * 3;
                player->RecalculateStats();
                player->hp = player->maxHp;
            }
        }

        ImGui::End();
    }
    rlImGuiEnd();

    EndDrawing();
}