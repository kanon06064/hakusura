#include "Game.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "raymath.h"
#include "rlgl.h"
#include "imgui.h"
#include "rlImGui.h"
#include "UI.h"
#include "DebugMenu.h"

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

    DrawTextEx(font, T("DEBUG_ROOM_TITLE", "デバッグルーム (TAB/ESCで戻る)").c_str(), { 20, 20 }, 20, 1, WHITE);
    DrawTextEx(font, T("DEBUG_CAM_INFO", "WASD + マウスでカメラ移動").c_str(), { 20, 50 }, 20, 1, LIGHTGRAY);

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
    else if (state == STATE_DEBUG_ROOM) {
        DrawDebugRoom();
    }
    else if (state == STATE_GAMEOVER) {
        ClearBackground(BLACK);
        DrawTextEx(font, T("DEATH_TITLE", "YOU DIED").c_str(), { (float)screenWidth / 2 - 100, (float)screenHeight / 2 - 50 }, 60, 2, RED);
        DrawTextEx(font, T("DEATH_DESC", "素材と消耗品を失って帰還します...").c_str(), { (float)screenWidth / 2 - 180, (float)screenHeight / 2 + 30 }, 24, 1, WHITE);
        DrawTextEx(font, T("CLICK_TO_CONT", "クリックして続行").c_str(), { (float)screenWidth / 2 - 80, (float)screenHeight / 2 + 80 }, 20, 1, LIGHTGRAY);
    }
    else if (state == STATE_GAMECLEAR) {
        ClearBackground(RAYWHITE);
        DrawTextEx(font, T("CLEAR_TITLE", "GAME CLEAR!!").c_str(), { (float)screenWidth / 2 - 150, (float)screenHeight / 2 - 60 }, 60, 2, GOLD);
        DrawTextEx(font, T("CLEAR_DESC", "魔王を討伐しました！").c_str(), { (float)screenWidth / 2 - 120, (float)screenHeight / 2 + 20 }, 24, 1, BLACK);
        DrawTextEx(font, T("CLICK_TO_RETURN", "クリックして拠点へ戻る").c_str(), { (float)screenWidth / 2 - 100, (float)screenHeight / 2 + 70 }, 20, 1, DARKGRAY);
    }
    else {
        ClearBackground(BLACK);
        BeginMode3D(camera);

        bool isBossFloor = (!isPortfolioMode && floor > 0 && floor % 10 == 0) || (isPortfolioMode && (floor == 2 || floor == 3));
        bool showExit = !isBossFloor || bossDefeated;

        dungeon.Draw(showExit);
        fxManager.Draw();

        // ドロップアイテムの3Dモデル描画
        for (auto& item : droppedItems) {
            if (!debugMode && !dungeon.IsDiscovered(item.pos.x, item.pos.z)) continue;

            Color rarityCol = Player::GetItemRarityColor(item.data);
            int tier = Player::GetItemTier(item.data);

            std::string mKey = item.data.modelName;

            if (item.data.type == "EQUIP") {
                if (mKey.empty()) {
                    if (DataManager::loadedModels.count("Wpn_" + std::to_string(item.data.id))) {
                        mKey = "Wpn_" + std::to_string(item.data.id);
                    }
                    else {
                        if (item.data.weaponSubtype == SWORD) mKey = "Wpn_Sword";
                        else if (item.data.weaponSubtype == SPEAR) mKey = "Wpn_Spear";
                        else if (item.data.weaponSubtype == AXE) mKey = "Wpn_Axe";
                        else if (item.data.weaponSubtype == WAND) mKey = "Wpn_Wand";
                    }
                }
            }

            rlPushMatrix();
            rlTranslatef(item.pos.x, item.pos.y, item.pos.z);
            rlRotatef(item.rotation, 0, 1, 0);

            if (!mKey.empty() && DataManager::loadedModels.count(mKey) > 0) {
                float drawScale = (item.data.type == "EQUIP") ? 0.7f : 0.6f;
                if (item.data.type == "EQUIP") {
                    rlRotatef(35.0f, 1, 0, 0);
                }
                // 自然な白色（ライティングシェーダーで立体的に描画）
                DrawModel(DataManager::loadedModels[mKey].model, { 0, 0, 0 }, drawScale, WHITE);
            }
            else {
                DrawCube({ 0,0,0 }, 0.45f, 0.35f, 0.45f, rarityCol);
                DrawCubeWires({ 0,0,0 }, 0.45f, 0.35f, 0.45f, WHITE);
            }
            rlPopMatrix();

            // 光の柱（Loot Beam）
            if (item.data.type == "EQUIP" || item.data.type == "ARMOR" || tier >= 3) {
                float time = (float)GetTime();
                float pulse = (sinf(time * 4.0f) * 0.5f + 0.5f);

                float beamHeight = 5.0f;
                float beamRadius = 0.18f;
                if (tier >= 4) {
                    beamHeight = 12.0f;
                    beamRadius = 0.28f;
                }

                Vector3 beamBase = { item.pos.x, 0.05f, item.pos.z };
                Vector3 beamTop = { item.pos.x, beamHeight, item.pos.z };

                DrawCylinderEx(beamBase, beamTop, 0.06f, 0.03f, 8, Fade(WHITE, 0.8f));
                float auraAlpha = 0.25f + pulse * 0.35f;
                DrawCylinderEx(beamBase, beamTop, beamRadius, beamRadius * 0.6f, 12, Fade(rarityCol, auraAlpha));

                float ringR = (beamRadius * 2.0f) + pulse * 0.3f;
                DrawCylinder(beamBase, ringR, ringR, 0.02f, 20, Fade(rarityCol, 0.3f));
                DrawCylinderWires(beamBase, ringR, ringR, 0.03f, 20, Fade(WHITE, 0.7f));
            }
        }

        for (auto& e : enemies) {
            if (debugMode || dungeon.IsDiscovered(e.position.x, e.position.z)) {
                e.Draw(debugMode, camera, font, player->position);
            }
        }

        player->Draw(debugMode);
        EndMode3D();

        if (state == STATE_DUNGEON && player) {
            for (const auto& spot : dungeon.gatherSpots) {
                if (spot.gathered) continue;
                if (!dungeon.IsDiscovered(spot.pos.x, spot.pos.z)) continue;

                float dist = Vector3Distance(player->position, spot.pos);
                if (dist < 2.2f) {
                    Vector2 sPos = GetWorldToScreen(Vector3Add(spot.pos, { 0, 1.2f, 0 }), camera);
                    if (sPos.x > 0 && sPos.y > 0) {
                        std::string gTxt = T("GATHER_PROMPT", "素材を採取する");
                        Vector2 gSize = MeasureTextEx(font, gTxt.c_str(), 18, 1);
                        DrawRectangle(sPos.x - gSize.x / 2 - 8, sPos.y - 4, gSize.x + 16, 26, Fade(BLACK, 0.75f));
                        DrawRectangleLines(sPos.x - gSize.x / 2 - 8, sPos.y - 4, gSize.x + 16, 26, spot.color);
                        DrawTextEx(font, gTxt.c_str(), { sPos.x - gSize.x / 2, sPos.y }, 18, 1, WHITE);
                    }
                }
            }
        }

        if (fxManager.damageFlashTimer > 0.0f) {
            float alpha = (fxManager.damageFlashTimer / 0.35f);
            if (alpha > 1.0f) alpha = 1.0f;

            DrawRectangle(0, 0, screenWidth, screenHeight, Fade(RED, alpha * 0.35f));

            int border = 20;
            DrawRectangle(0, 0, screenWidth, border, Fade(MAROON, alpha * 0.6f));
            DrawRectangle(0, screenHeight - border, screenWidth, border, Fade(MAROON, alpha * 0.6f));
            DrawRectangle(0, 0, border, screenHeight, Fade(MAROON, alpha * 0.6f));
            DrawRectangle(screenWidth - border, 0, border, screenHeight, Fade(MAROON, alpha * 0.6f));
        }

        fxManager.Draw2D(font, camera);
        UI::DrawLogs(logs, *player, camera, font);
        UI::DrawNearbyItems(*player, droppedItems, dungeon, camera, font);

        int displayFloor = isPortfolioMode ? (floor + 1000) : floor;

        UI::DrawHUD(*player, enemies, dungeon, camera, displayFloor, currentDungeonId, debugMode, font);

        for (const auto& e : enemies) {
            if (e.isBoss && e.hp > 0 && !e.isDead) {
                UI::DrawBossHUD(e, font);
                break;
            }
        }

        bool isFinalBoss = isPortfolioMode ? (floor == 3) : (currentDungeonId == 2 && floor == 100);
        if (fxManager.victoryBannerTimer > 0.0f) {
            UI::DrawBossDefeatedBanner(fxManager.victoryBannerTimer, font, isFinalBoss);
        }

        UI::UpdateSystemLogs(GetFrameTime());
        UI::DrawSystemLogs(font);

        if (showMenu) {
            int menuEvent = UI::DrawMenu(*player, dungeon, currentTab, font);

            if (menuEvent == 1) {
                SaveCurrentSlot();
                UI::AddSystemLog(T("LOG_GAME_SAVED", "ゲームをセーブしました！"), GREEN);
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

            auto dist2D = [](Vector3 a, Vector3 b) { return Vector2Distance({ a.x, a.z }, { b.x, b.z }); };

            if (state == STATE_HOME && hoveredEntranceIndex != -1) m = "ENTER_DUNGEON";
            else if (state == STATE_DUNGEON) {
                if (!isPortfolioMode && floor == maxF && dist2D(player->position, dungeon.portalPos) < 2.0f) m = "RETURN_HOME";
                else if (dungeon.stairsDownPos.x != -999 && dist2D(player->position, dungeon.stairsDownPos) < 2.0f) m = "GO_DEEPER";
                else if (dungeon.stairsUpPos.x != -999 && dist2D(player->position, dungeon.stairsUpPos) < 2.0f) m = "RETURN_HOME";
                else if (dungeon.portalPos.x != -999 && dist2D(player->position, dungeon.portalPos) < 2.0f) m = "RETURN_HOME";
            }

            int res = UI::DrawPrompt(m, screenWidth, screenHeight, font);

            if (IsGamepadAvailable(0)) {
                if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) res = 1;
                if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) res = 2;
            }

            if (res == 1) {
                if (state == STATE_HOME && hoveredEntranceIndex != -1) {
                    currentDungeonId = hoveredEntranceIndex;
                    floor = 0;
                    NextFloor();
                }
                else if (state == STATE_DUNGEON && dungeon.stairsDownPos.x != -999 && dist2D(player->position, dungeon.stairsDownPos) < 2.0f) {
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
        DebugMenu::Draw(this);
    }
    rlImGuiEnd();

    EndDrawing();
}