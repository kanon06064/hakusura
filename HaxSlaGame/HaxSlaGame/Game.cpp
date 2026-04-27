#include "Game.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "raymath.h"
#include <iostream>
#include <time.h>
#include <algorithm>

Game::Game()
    : screenWidth(1280), screenHeight(720), player(nullptr), camera({ 0 }), state(STATE_TITLE),
    floor(0), currentDungeonId(0), unlockedDungeonId(0), currentSlot(0),
    debugMode(false), showMenu(false), showStorage(false), showReforgeMenu(false), showWarpMenu(false),
    showCraftMenu(false), showQuestMenu(false),
    showPrompt(false), currentTab(EQUIP), sceneTimer(0.0f), bossDefeated(false), isPortfolioMode(false)
{
    InitWindow(screenWidth, screenHeight, "3D Hack and Slash RPG Refactored");

    rlImGuiSetup(true);

    AudioManager::Init();

    SetTargetFPS(60);
    DataManager::LoadAllData();
    maxFloors = { 0, 0, 0 };

    std::vector<int> cps;
    for (int i = 32; i < 127; i++) cps.push_back(i);
    for (int i = 0x3000; i <= 0x30FF; i++) cps.push_back(i);
    for (int i = 0x4E00; i <= 0x9FAF; i++) cps.push_back(i);
    for (int i = 0xFF00; i <= 0xFFEF; i++) cps.push_back(i);

    font = LoadFontEx("jp_font.ttf", 32, cps.data(), (int)cps.size());
    if (font.texture.id == 0) font = LoadFontEx("C:/Windows/Fonts/msgothic.ttc", 32, cps.data(), (int)cps.size());
    if (font.texture.id == 0) font = GetFontDefault(); else SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    AudioManager::PlayBGM(BGM_TITLE);
}

Game::~Game() {
    if (player) delete player;
    UnloadFont(font);
    DataManager::UnloadAllData();
    AudioManager::Close();

    rlImGuiShutdown();
    CloseWindow();
}

void Game::InitGame() {
    floor = 0;
    currentDungeonId = 0;
    unlockedDungeonId = 0;
    maxFloors = { 0, 0, 0 };

    state = STATE_HOME; bossDefeated = false; dungeon.Generate(true, 0);
    isPortfolioMode = false;
    if (player) delete player;
    Vector3 startPos = dungeon.GetStartPosition(); startPos.y = 0.5f; player = new Player(startPos);
    camera = { {player->position.x + 10.0f, 15.0f, player->position.z + 10.0f}, player->position, {0.0f, 1.0f, 0.0f}, 45.0f, 0 };
    fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear(); enemies.clear(); droppedItems.clear(); storageItems.clear(); storageEquip.clear();
    debugMode = false; showMenu = false; showStorage = false; showReforgeMenu = false; showWarpMenu = false; showCraftMenu = false; showQuestMenu = false; showPrompt = false; currentTab = EQUIP; sceneTimer = 0;

    AudioManager::PlayBGM(BGM_HOME);
}

void Game::StartPortfolioMode() {
    InitGame();
    isPortfolioMode = true;
    currentSlot = 3;

    player->level = 99;
    player->gold = 999999;
    player->skillPoints = 9999;
    for (auto& node : player->skillTree) node.unlocked = true;

    for (const auto& item : DataManager::itemConfigs) {
        ItemData i = item;
        if (i.type == "EQUIP" || i.type == "ARMOR") {
            i.modifierId = 7;
            player->inventoryEquip.push_back(i);
        }
        else {
            i.count = 99;
            player->inventoryItems.push_back(i);
        }
    }

    for (auto& eq : player->inventoryEquip) {
        if (eq.id == 403) { player->equippedData[0] = eq; player->equippedWeapons[0] = AXE; }
        if (eq.id == 405) { player->equippedData[1] = eq; player->equippedWeapons[1] = WAND; }
        if (eq.id == 540) player->equippedArmor[0] = eq;
        if (eq.id == 541) player->equippedArmor[1] = eq;
        if (eq.id == 542) player->equippedArmor[2] = eq;
        if (eq.id == 543) player->equippedArmor[3] = eq;
        if (eq.id == 544) player->equippedArmor[4] = eq;
    }
    player->currentWeapon = player->equippedWeapons[0];
    player->RecalculateStats();
    player->hp = player->maxHp;

    unlockedDungeonId = 2;
    maxFloors = { 30, 50, 100 };

    SaveCurrentSlot();
    logs.insert(logs.begin(), { "RUSH MODE STARTED!", 5.0f, ORANGE });
}

void Game::InitDebugRoom() {
    floor = -1;
    state = STATE_DEBUG_ROOM;
    enemies.clear();

    camera.position = { 10.0f, 10.0f, 20.0f };
    camera.target = { 10.0f, 0.0f, 0.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    int x = 0;
    for (const auto& data : DataManager::allEnemyData) {
        for (int i = 0; i < 4; i++) {
            Vector3 pos = { (float)x * 2.5f, 0.5f, (float)i * 2.5f };
            Enemy e(pos, data, 1);
            if (i == 0) e.state = STATE_PATROL;
            if (i == 1) e.state = STATE_CHASE;
            if (i == 2) e.state = STATE_ATTACK;
            if (i == 3) e.StartDeath();
            enemies.push_back(e);
        }
        x++;
    }
}

void Game::StartDebugRoom() {
    if (player) delete player;
    player = new Player({ 0,0,0 });
    InitDebugRoom();
}

void Game::LoadAndStart(int slot) {
    currentSlot = slot; InitGame();
    if (DataManager::LoadGame(slot, player, floor, currentDungeonId, unlockedDungeonId, maxFloors, storageItems, storageEquip, isPortfolioMode)) {
        if (floor == 0) {
            state = STATE_HOME; dungeon.Generate(true, 0);
            AudioManager::PlayBGM(BGM_HOME);
        }
        else {
            state = STATE_DUNGEON; dungeon.Generate(false, floor); SpawnEnemies(10 + floor);
            AudioManager::PlayBGM(BGM_DUNGEON);
        }
        Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    }
    else { InitGame(); }
}

void Game::NewGameAndStart(int slot) { currentSlot = slot; InitGame(); }

void Game::SaveCurrentSlot() {
    if (player) {
        DataManager::SaveGame(currentSlot, player, floor, currentDungeonId, unlockedDungeonId, maxFloors, storageItems, storageEquip, isPortfolioMode);
        AudioManager::PlaySE(SE_SAVE);
    }
}

void Game::SpawnEnemies(int count) {
    if (state == STATE_HOME) return; enemies.clear();

    if (isPortfolioMode) {
        if (floor == 1) {
            for (int i = 0; i < count; i++) {
                enemies.push_back(Enemy(dungeon.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(30, 0), 30));
            }
        }
        else if (floor == 2) {
            if (dungeon.bossSpawnPos.x != -999) {
                EnemyData bossData = DataManager::GetBossEnemy(30, 0);
                Enemy boss(dungeon.bossSpawnPos, bossData, 50);
                boss.maxHp *= 3.0f; boss.hp = boss.maxHp; boss.radius = 1.5f;
                boss.isBoss = true;
                enemies.push_back(boss);
            }
            bossDefeated = false;
        }
        else if (floor == 3) {
            if (dungeon.bossSpawnPos.x != -999) {
                EnemyData bossData = DataManager::GetBossEnemy(100, 2);
                Enemy boss(dungeon.bossSpawnPos, bossData, 100);
                boss.maxHp *= 3.0f; boss.hp = boss.maxHp; boss.radius = 1.5f;
                boss.isBoss = true;
                enemies.push_back(boss);
            }
            bossDefeated = false;
        }
        return;
    }

    if (floor > 0 && floor % 10 == 5) return;

    int enemyLevel = floor;
    if (currentDungeonId == 1) enemyLevel += 30;
    if (currentDungeonId == 2) enemyLevel += 60;

    if (floor > 0 && floor % 10 == 0) {
        if (dungeon.bossSpawnPos.x != -999) {
            EnemyData bossData = DataManager::GetBossEnemy(floor, currentDungeonId);
            Enemy boss(dungeon.bossSpawnPos, bossData, enemyLevel);
            boss.maxHp *= 3.0f; boss.hp = boss.maxHp; boss.radius = 1.5f;
            boss.isBoss = true;
            enemies.push_back(boss);
        }
        bossDefeated = false; return;
    }
    for (int i = 0; i < count; i++) {
        enemies.push_back(Enemy(dungeon.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(floor, currentDungeonId), enemyLevel));
    }
}

void Game::Run() { while (!WindowShouldClose()) { Update(); Draw(); } }

void Game::UpdateDebugRoom() {
    AudioManager::Update();
    UpdateCamera(&camera, CAMERA_FREE);
    for (auto& e : enemies) e.animFrameCounter++;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB)) {
        state = STATE_TITLE;
        enemies.clear();
        AudioManager::PlayBGM(BGM_TITLE);
    }
}

void Game::DrawDebugRoom() {
    ClearBackground(BLACK);
    BeginMode3D(camera);
    DrawGrid(100, 1.0f);
    for (auto& e : enemies) e.Draw(true, camera, font, camera.position);
    EndMode3D();

    DrawTextEx(font, u8"デバッグルーム (TAB/ESCで戻る)", { 20, 20 }, 20, 1, WHITE);
    DrawTextEx(font, u8"WASD + マウスでカメラ移動", { 20, 50 }, 20, 1, LIGHTGRAY);

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

void Game::Update() {
    AudioManager::Update();

    if (state == STATE_TITLE) return;
    if (state == STATE_DEBUG_ROOM) { UpdateDebugRoom(); return; }

    float dt = GetFrameTime();

    if (state == STATE_GAMEOVER || state == STATE_GAMECLEAR) {
        if (IsMouseButtonPressed(0) || IsKeyPressed(KEY_SPACE)) {
            ApplyDeathPenalty();
        }
        return;
    }

    if (IsKeyPressed(KEY_F1)) debugMode = !debugMode;
    if (IsKeyPressed(KEY_TAB)) { if (!UI::showDetail) { showMenu = !showMenu; AudioManager::PlaySE(SE_CLICK); } }
    if (debugMode) { if (IsKeyPressed(KEY_N)) NextFloor(); if (IsKeyPressed(KEY_R)) ReturnHome(); }

    bool stopPlayer = showMenu || showPrompt || showStorage || showReforgeMenu || showWarpMenu || showCraftMenu || showQuestMenu;
    if (UI::showDetail) stopPlayer = true;

    Vector3 offset = Vector3Subtract(camera.position, camera.target);
    camera.target = player->position;
    camera.position = Vector3Add(player->position, offset);

    if (!stopPlayer) {
        if (IsMouseButtonDown(1) || GetMouseWheelMove() != 0) {
            UpdateCamera(&camera, CAMERA_THIRD_PERSON);

            Vector3 camOffset = Vector3Subtract(camera.position, camera.target);
            float dist = Vector3Length(camOffset);
            float horizontalDist = sqrtf(camOffset.x * camOffset.x + camOffset.z * camOffset.z);
            float pitch = atan2f(camOffset.y, horizontalDist);

            float minPitch = 20.0f * DEG2RAD;
            float maxPitch = 75.0f * DEG2RAD;

            bool camChanged = false;
            if (pitch < minPitch) { pitch = minPitch; camChanged = true; }
            if (pitch > maxPitch) { pitch = maxPitch; camChanged = true; }

            float minDist = 5.0f;
            float maxDist = 30.0f;
            if (dist < minDist) { dist = minDist; camChanged = true; }
            if (dist > maxDist) { dist = maxDist; camChanged = true; }

            if (camChanged) {
                float yaw = atan2f(camOffset.x, camOffset.z);
                float newHorizontal = dist * cosf(pitch);
                camOffset.y = dist * sinf(pitch);
                camOffset.x = newHorizontal * sinf(yaw);
                camOffset.z = newHorizontal * cosf(yaw);
                camera.position = Vector3Add(camera.target, camOffset);
            }
        }
    }

    player->Update(camera, dungeon, enemies, fxManager, stopPlayer);
    fxManager.Update(dt, dungeon); fxManager.CheckProjectileCollisions(enemies, *player, dungeon); dungeon.UpdateVisibility(player->position);

    if (!stopPlayer) {
        if (player->hp <= 0 && state != STATE_HOME) {
            state = STATE_GAMEOVER;
            AudioManager::PlayBGM(BGM_NONE);
        }

        for (int i = (int)enemies.size() - 1; i >= 0; i--) {
            enemies[i].Update(*player, dungeon, fxManager);

            if (enemies[i].hp <= 0 && !enemies[i].isDying) {
                player->AddExp(enemies[i].expValue, fxManager);
                player->gold += enemies[i].data.gold;

                player->UpdateHuntQuest(enemies[i].data.id);

                for (int id : enemies[i].data.drops) {
                    ItemData cfg = DataManager::GetItemConfigCopy(id);
                    if (cfg.id != -1) {
                        float chance = cfg.dropChance;
                        if ((float)GetRandomValue(0, 10000) / 10000.0f <= chance) {
                            if (cfg.type == "EQUIP" || cfg.type == "ARMOR") cfg.modifierId = DataManager::GetRandomModifierId();

                            // ★修正: ドロップアイテムの物理初期パラメータを設定
                            DroppedItem di;
                            di.pos = enemies[i].position;
                            di.pos.y += 0.5f; // 敵の腰の高さから出現
                            di.data = cfg;
                            // 上方向 + ランダムなXY方向へ飛び出す
                            di.vel = { (float)GetRandomValue(-20, 20) * 0.01f, 0.4f, (float)GetRandomValue(-20, 20) * 0.01f };
                            di.rotation = (float)GetRandomValue(0, 360);

                            droppedItems.push_back(di);
                        }
                    }
                }
                enemies[i].StartDeath();
            }
            if (enemies[i].isDead) { enemies.erase(enemies.begin() + i); }
        }

        // ★追加: ドロップアイテムの物理挙動を更新（放物線とバウンド）
        for (auto& item : droppedItems) {
            item.pos = Vector3Add(item.pos, item.vel);
            item.vel.y -= 2.0f * dt; // 重力
            if (item.pos.y <= 0.2f) { // 地面に接触
                item.pos.y = 0.2f;
                item.vel.y *= -0.5f; // バウンド
                item.vel.x *= 0.8f;  // 摩擦
                item.vel.z *= 0.8f;
                if (fabsf(item.vel.y) < 0.05f) item.vel.y = 0;
            }
            // 空中や移動中は回転させる
            item.rotation += 100.0f * dt * (fabsf(item.vel.x) + fabsf(item.vel.z));
        }

        if (isPortfolioMode) {
            if ((floor == 2 || floor == 3) && enemies.empty() && !bossDefeated) {
                bossDefeated = true;
                logs.insert(logs.begin(), { "BOSS DEFEATED!", 5.0f, GOLD });
                if (floor == 3) {
                    state = STATE_GAMECLEAR;
                    AudioManager::PlayBGM(BGM_NONE);
                }
            }
        }
        else {
            if (floor > 0 && floor % 10 == 0 && enemies.empty() && !bossDefeated) {
                bossDefeated = true;

                int maxF = (currentDungeonId == 0) ? 30 : (currentDungeonId == 1) ? 50 : 100;

                if (floor == maxF) {
                    if (currentDungeonId == unlockedDungeonId && unlockedDungeonId < 2) {
                        unlockedDungeonId++;
                        logs.insert(logs.begin(), { "NEXT DUNGEON UNLOCKED!", 5.0f, LIME });
                    }
                    if (currentDungeonId == 2) {
                        state = STATE_GAMECLEAR;
                        AudioManager::PlayBGM(BGM_NONE);
                    }
                    else {
                        logs.insert(logs.begin(), { "DUNGEON CLEARED!", 5.0f, GOLD });
                    }
                }
                else {
                    logs.insert(logs.begin(), { "BOSS DEFEATED!", 5.0f, GOLD });
                }
            }
        }

        for (int i = (int)droppedItems.size() - 1; i >= 0; i--) {
            if (Vector3Distance(player->position, droppedItems[i].pos) < 1.0f) {
                if (player->AddToInventory(droppedItems[i].data)) {
                    UI::AddSystemLog(Player::GetFullItemName(droppedItems[i].data) + u8" を入手した", Player::GetItemRarityColor(droppedItems[i].data)); // ★レアリティ色でログ
                    droppedItems.erase(droppedItems.begin() + i);
                    AudioManager::PlaySE(SE_CLICK);
                }
            }
        }

        if (sceneTimer > 0) sceneTimer -= dt;
        else {
            bool canUseStairs = true;
            int maxF = (currentDungeonId == 0) ? 30 : (currentDungeonId == 1) ? 50 : 100;

            if (!isPortfolioMode && floor > 0 && floor % 10 == 0 && !bossDefeated) canUseStairs = false;
            if (isPortfolioMode && (floor == 2 || floor == 3) && !bossDefeated) canUseStairs = false;

            if (canUseStairs && Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) {
                if (floor == maxF) showPrompt = true;
                else showPrompt = true;
            }
            if (Vector3Distance(player->position, dungeon.stairsUpPos) < 2.0f) showPrompt = true;
            if (state == STATE_DUNGEON && dungeon.portalPos.x != -999 && Vector3Distance(player->position, dungeon.portalPos) < 2.0f) showPrompt = true;
        }

        if (dungeon.healStationPos.x != -999 && Vector3Distance(player->position, dungeon.healStationPos) < 2.0f) {
            if (player->hp < player->maxHp) { player->hp += 50.0f * dt; if (player->hp > player->maxHp) player->hp = player->maxHp; if (GetRandomValue(0, 10) < 2) fxManager.SpawnEffect(Vector3Add(player->position, { 0,1,0 }), { 0,1,0 }, FX_HIT, GREEN); }
        }

        if (state == STATE_HOME && !isPortfolioMode) {
            if (Vector3Distance(player->position, dungeon.storageBoxPos) < 2.0f && IsMouseButtonPressed(0)) { showStorage = true; AudioManager::PlaySE(SE_CLICK); }
            if (Vector3Distance(player->position, dungeon.reforgeStationPos) < 2.0f && IsMouseButtonPressed(0)) { showReforgeMenu = true; AudioManager::PlaySE(SE_CLICK); }
            if (dungeon.portalPos.x != -999 && Vector3Distance(player->position, dungeon.portalPos) < 2.0f && IsMouseButtonPressed(0)) { showWarpMenu = true; AudioManager::PlaySE(SE_CLICK); }
            if (dungeon.craftStationPos.x != -999 && Vector3Distance(player->position, dungeon.craftStationPos) < 2.0f && IsMouseButtonPressed(0)) { showCraftMenu = true; AudioManager::PlaySE(SE_CLICK); }

            if (dungeon.questBoardPos.x != -999 && Vector3Distance(player->position, dungeon.questBoardPos) < 2.0f && IsMouseButtonPressed(0)) {
                showQuestMenu = true; AudioManager::PlaySE(SE_CLICK);
            }
        }
    }
    for (int i = (int)logs.size() - 1; i >= 0; i--) { logs[i].life -= dt; if (logs[i].life <= 0) logs.erase(logs.begin() + i); }
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
        DrawTextEx(font, "YOU DIED", { (float)screenWidth / 2 - 100, (float)screenHeight / 2 - 50 }, 60, 2, RED);
        DrawTextEx(font, u8"素材と消耗品を失って帰還します...", { (float)screenWidth / 2 - 180, (float)screenHeight / 2 + 30 }, 24, 1, WHITE);
        DrawTextEx(font, "Click to Continue", { (float)screenWidth / 2 - 80, (float)screenHeight / 2 + 80 }, 20, 1, LIGHTGRAY);
    }
    else if (state == STATE_GAMECLEAR) {
        ClearBackground(RAYWHITE);
        DrawTextEx(font, "GAME CLEAR!!", { (float)screenWidth / 2 - 150, (float)screenHeight / 2 - 60 }, 60, 2, GOLD);
        DrawTextEx(font, u8"魔王を討伐しました！", { (float)screenWidth / 2 - 120, (float)screenHeight / 2 + 20 }, 24, 1, BLACK);
        DrawTextEx(font, "Click to Return Home", { (float)screenWidth / 2 - 100, (float)screenHeight / 2 + 70 }, 20, 1, DARKGRAY);
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

        // ★修正: アイテムのドロップ描画（回転とレアリティの光の柱）
        for (auto& item : droppedItems) {
            if (!debugMode && !dungeon.IsDiscovered(item.pos.x, item.pos.z)) continue;

            Color rarityCol = Player::GetItemRarityColor(item.data);

            rlPushMatrix();
            rlTranslatef(item.pos.x, item.pos.y, item.pos.z);
            rlRotatef(item.rotation, 0, 1, 0); // 回転

            // アイテム本体を描画
            DrawCube({ 0,0,0 }, 0.5f, 0.4f, 0.5f, rarityCol);
            DrawCubeWires({ 0,0,0 }, 0.5f, 0.4f, 0.5f, WHITE);
            rlPopMatrix();

            // レアリティに応じた光の柱を描画（装備品のみ）
            if (item.data.type == "EQUIP" || item.data.type == "ARMOR") {
                Color pillarCol = rarityCol;
                // 時間でふわふわ明滅させる
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
        UI::DrawHUD(*player, enemies, dungeon, camera, displayFloor, debugMode, font);
        UI::DrawLogs(logs, *player, camera, font);
        UI::DrawNearbyItems(*player, droppedItems, dungeon, camera, font);

        UI::UpdateSystemLogs(GetFrameTime());
        UI::DrawSystemLogs(font);

        if (showMenu) {
            int menuEvent = UI::DrawMenu(*player, dungeon, currentTab, font);

            if (menuEvent == 1) {
                SaveCurrentSlot();
                UI::AddSystemLog("GAME SAVED!", GREEN);
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

        if (showWarpMenu) {
            UI::DrawWarpMenu(this, unlockedDungeonId, maxFloors, font, showWarpMenu);
        }

        if (showCraftMenu) UI::DrawCraftingMenu(*player, font, showCraftMenu);

        if (showQuestMenu) {
            UI::DrawQuestMenu(*player, font, showQuestMenu);
        }

        if (showPrompt) {
            int maxF = (currentDungeonId == 0) ? 30 : (currentDungeonId == 1) ? 50 : 100;
            const char* m = "UNKNOWN";

            if (floor == 0) m = "ENTER_DUNGEON";
            else if (floor == maxF) m = "RETURN_HOME";
            else if (Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) m = "GO_DEEPER";
            else if (Vector3Distance(player->position, dungeon.stairsUpPos) < 2.0f) m = "RETURN_HOME";
            else if (dungeon.portalPos.x != -999 && Vector3Distance(player->position, dungeon.portalPos) < 2.0f) m = "RETURN_HOME";

            int res = UI::DrawPrompt(m, screenWidth, screenHeight, font);
            if (res == 1) {
                if (floor == 0) {
                    currentDungeonId = 0;
                    NextFloor();
                }
                else if (floor > 0 && floor != maxF && Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) {
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

void Game::ApplyDeathPenalty() {
    if (isPortfolioMode) {
        state = STATE_TITLE;
        isPortfolioMode = false;
        AudioManager::PlayBGM(BGM_TITLE);
        return;
    }

    if (state == STATE_GAMEOVER) {
        player->inventoryItems.clear();
        logs.clear();
        logs.insert(logs.begin(), { "Returned to Home...", 5.0f, WHITE });
        logs.insert(logs.begin(), { "Lost materials.", 5.0f, RED });
    }
    else {
        logs.clear();
        logs.insert(logs.begin(), { "CONGRATULATIONS!", 5.0f, GOLD });
        logs.insert(logs.begin(), { "Returned to Home.", 5.0f, WHITE });
    }

    player->hp = player->maxHp;
    ReturnHome();
}

void Game::NextFloor() {
    if (isPortfolioMode) {
        floor++;
        state = STATE_DUNGEON; bossDefeated = false;

        if (floor == 1) {
            dungeon.Generate(false, 1);
            SpawnEnemies(15);
        }
        else if (floor == 2) {
            dungeon.Generate(false, 10);
            SpawnEnemies(0);
        }
        else if (floor == 3) {
            dungeon.Generate(false, 100);
            SpawnEnemies(0);
        }
        Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
        droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();
        AudioManager::PlayBGM(BGM_DUNGEON);
        return;
    }

    floor++;
    if (floor > maxFloors[currentDungeonId]) maxFloors[currentDungeonId] = floor;

    state = STATE_DUNGEON; bossDefeated = false;
    dungeon.Generate(false, floor);
    Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();

    int enemyLevel = floor;
    if (currentDungeonId == 1) enemyLevel += 30;
    if (currentDungeonId == 2) enemyLevel += 60;
    SpawnEnemies(10 + floor);

    AudioManager::PlayBGM(BGM_DUNGEON);

    if (floor % 10 != 0 && floor % 10 != 5) {
        std::vector<int> candidateItemIds;
        EnemyData currentEnemy = DataManager::GetRandomEnemyForFloor(enemyLevel, currentDungeonId); for (int id : currentEnemy.drops) candidateItemIds.push_back(id);
        EnemyData nextEnemy = DataManager::GetRandomEnemyForFloor(enemyLevel + 1, currentDungeonId); for (int id : nextEnemy.drops) candidateItemIds.push_back(id);
        for (const auto& pos : dungeon.treasureSpots) {
            ItemData item;
            if (!candidateItemIds.empty()) { int rndIdx = GetRandomValue(0, (int)candidateItemIds.size() - 1); item = DataManager::GetItemConfigCopy(candidateItemIds[rndIdx]); }
            if (item.id == -1 && !DataManager::itemConfigs.empty()) { int rndId = GetRandomValue(0, (int)DataManager::itemConfigs.size() - 1); item = DataManager::itemConfigs[rndId]; }

            if (item.id != -1) {
                if (item.type == "EQUIP" || item.type == "ARMOR") item.modifierId = DataManager::GetRandomModifierId();
                // 初期配置アイテムも物理パラメーターを持たせる
                DroppedItem di;
                di.pos = pos;
                di.pos.y = 0.2f;
                di.vel = { 0,0,0 };
                di.rotation = (float)GetRandomValue(0, 360);
                di.data = item;
                droppedItems.push_back(di);
            }
        }
    }
}

void Game::WarpToFloor(int targetDungeon, int targetFloor) {
    currentDungeonId = targetDungeon;
    floor = targetFloor;
    state = STATE_DUNGEON; bossDefeated = false;

    dungeon.Generate(false, floor);
    Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();

    int enemyLevel = floor;
    if (currentDungeonId == 1) enemyLevel += 30;
    if (currentDungeonId == 2) enemyLevel += 60;
    SpawnEnemies(10 + floor);

    showWarpMenu = false;
    AudioManager::PlayBGM(BGM_DUNGEON);
    AudioManager::PlaySE(SE_STAIRS);
}

void Game::ReturnHome() {
    floor = 0; state = STATE_HOME; bossDefeated = false;
    dungeon.Generate(true, 0);
    Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    enemies.clear(); droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();

    AudioManager::PlayBGM(BGM_HOME);
    AudioManager::PlaySE(SE_STAIRS);
}