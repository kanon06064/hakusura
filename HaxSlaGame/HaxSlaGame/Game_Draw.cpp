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
    else if (state == STATE_DEBUG_ROOM) {
        DrawDebugRoom();
    }
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

        // ★ 修正: ボスフロア判定を行い、ボス撃破前なら出口（階段・クリアポータル）を描画しない
        bool isBossFloor = (!isPortfolioMode && floor > 0 && floor % 10 == 0) || (isPortfolioMode && (floor == 2 || floor == 3));
        bool showExit = !isBossFloor || bossDefeated;

        dungeon.Draw(showExit);

        // ※ かつて存在していた「黒いキューブを描画して隠す（DrawCube BLACK）」処理は完全削除

        fxManager.Draw();

        for (auto& item : droppedItems) {
            if (!debugMode && !dungeon.IsDiscovered(item.pos.x, item.pos.z)) continue;

            Color rarityCol = Player::GetItemRarityColor(item.data);
            int tier = Player::GetItemTier(item.data);

            // アイテム箱本体（回転しながら浮遊）
            rlPushMatrix();
            rlTranslatef(item.pos.x, item.pos.y, item.pos.z);
            rlRotatef(item.rotation, 0, 1, 0);
            DrawCube({ 0,0,0 }, 0.45f, 0.35f, 0.45f, rarityCol);
            DrawCubeWires({ 0,0,0 }, 0.45f, 0.35f, 0.45f, WHITE);
            rlPopMatrix();

            // ★ 光の柱（Loot Beam）の描画
            if (item.data.type == "EQUIP" || item.data.type == "ARMOR" || tier >= 3) {
                float time = (float)GetTime();
                float pulse = (sinf(time * 4.0f) * 0.5f + 0.5f); // 0.0 〜 1.0 の脈動

                // レア度に応じて光の高さと太さを変える
                float beamHeight = 5.0f;
                float beamRadius = 0.18f;
                if (tier >= 4) {
                    beamHeight = 12.0f; // エピック・伝説は天高く伸びる！
                    beamRadius = 0.28f;
                }

                Vector3 beamBase = { item.pos.x, 0.05f, item.pos.z };
                Vector3 beamTop = { item.pos.x, beamHeight, item.pos.z };

                // 1. 白く輝く中心のコア光線
                DrawCylinderEx(beamBase, beamTop, 0.06f, 0.03f, 8, Fade(WHITE, 0.8f));

                // 2. 外側の半透明オーラ光柱
                float auraAlpha = 0.25f + pulse * 0.35f;
                DrawCylinderEx(beamBase, beamTop, beamRadius, beamRadius * 0.6f, 12, Fade(rarityCol, auraAlpha));

                // 3. 地面に広がる光のリング（波紋）
                float ringR = (beamRadius * 2.0f) + pulse * 0.3f;
                DrawCylinder(beamBase, ringR, ringR, 0.02f, 20, Fade(rarityCol, 0.3f));
                DrawCylinderWires(beamBase, ringR, ringR, 0.03f, 20, Fade(WHITE, 0.7f));
            }
        }



            for (auto& e : enemies) if (debugMode || dungeon.IsDiscovered(e.position.x, e.position.z)) e.Draw(debugMode, camera, font, player->position);

            player->Draw(debugMode);
            EndMode3D();

            if (fxManager.damageFlashTimer > 0.0f) {
                float alpha = (fxManager.damageFlashTimer / 0.35f);
                if (alpha > 1.0f) alpha = 1.0f;

                // 画面全体の薄い赤
                DrawRectangle(0, 0, screenWidth, screenHeight, Fade(RED, alpha * 0.35f));

                // 画面の四隅（枠）を強調して臨場感を出す
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
        // ★ 独立した DebugMenu を呼び出すだけになり、Game_Draw.cpp が超スッキリ！
        if (debugMode && state != STATE_TITLE) {
            DebugMenu::Draw(this);
        }
        rlImGuiEnd();

        EndDrawing();
    }
