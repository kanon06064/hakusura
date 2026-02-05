#include "Game.h"
#include "DataManager.h"
#include "raymath.h"
#include <iostream>
#include <time.h>
#include <algorithm>

Game::Game() {
    InitWindow(screenWidth, screenHeight, "3D Hack and Slash RPG Refactored");
    SetTargetFPS(60);
    DataManager::LoadAllData();

    std::vector<int> cps;
    for (int i = 32; i < 127; i++) cps.push_back(i);
    for (int i = 0x3000; i <= 0x30FF; i++) cps.push_back(i);
    for (int i = 0x4E00; i <= 0x9FAF; i++) cps.push_back(i);

    font = LoadFontEx("jp_font.ttf", 32, cps.data(), (int)cps.size());
    if (font.texture.id == 0) font = LoadFontEx("C:/Windows/Fonts/msgothic.ttc", 32, cps.data(), (int)cps.size());
    if (font.texture.id == 0) font = GetFontDefault();
    else SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);

    player = nullptr;
    InitGame();
}

Game::~Game() {
    if (player) delete player;
    UnloadFont(font);
    CloseWindow();
}

void Game::InitGame() {
    floor = 0;
    state = STATE_HOME;
    dungeon.Generate(true, 0);

    if (player) delete player;
    Vector3 startPos = dungeon.GetStartPosition();
    startPos.y = 0.5f;
    player = new Player(startPos);

    camera = { {player->position.x + 10.0f, 15.0f, player->position.z + 10.0f}, player->position, {0.0f, 1.0f, 0.0f}, 45.0f, 0 };

    fxManager.projectiles.clear();
    fxManager.effects.clear();
    fxManager.damageTexts.clear();
    enemies.clear();
    droppedItems.clear();

    debugMode = false;
    showMenu = false;
    showStorage = false;
    showReforgeMenu = false;
    showPrompt = false;
    currentTab = EQUIP;
    sceneTimer = 0;
}

void Game::SpawnEnemies(int count) {
    if (state == STATE_HOME) return;
    enemies.clear();
    for (int i = 0; i < count; i++) {
        enemies.push_back(Enemy(dungeon.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(floor), floor));
    }
}

void Game::Run() {
    while (!WindowShouldClose()) {
        Update();
        Draw();
    }
}

void Game::Update() {
    float dt = GetFrameTime();

    if (IsKeyPressed(KEY_F1)) debugMode = !debugMode;
    if (IsKeyPressed(KEY_TAB)) showMenu = !showMenu;
    if (debugMode) {
        if (IsKeyPressed(KEY_N)) NextFloor();
        if (IsKeyPressed(KEY_R)) ReturnHome();
    }

    bool stopPlayer = showMenu || showPrompt || showStorage || showReforgeMenu;

    Vector3 offset = Vector3Subtract(camera.position, camera.target);
    camera.target = player->position;
    camera.position = Vector3Add(player->position, offset);
    if (!stopPlayer) {
        if (IsMouseButtonDown(1) || GetMouseWheelMove() != 0) UpdateCamera(&camera, CAMERA_THIRD_PERSON);
    }

    player->Update(camera, dungeon, enemies, fxManager, stopPlayer);
    fxManager.Update(dt, dungeon);
    fxManager.CheckProjectileCollisions(enemies, *player, dungeon);
    dungeon.UpdateVisibility(player->position);

    if (!stopPlayer) {
        for (int i = (int)enemies.size() - 1; i >= 0; i--) {
            enemies[i].Update(*player, dungeon, fxManager);
            if (enemies[i].hp <= 0) {
                player->AddExp(enemies[i].expValue, fxManager);
                player->gold += enemies[i].data.gold;
                for (int id : enemies[i].data.drops) {
                    ItemData cfg = DataManager::GetItemConfigCopy(id);
                    if (cfg.id != -1 && (float)GetRandomValue(0, 1000) / 1000.0f < cfg.dropChance) {
                        if (cfg.type == "EQUIP") {
                            cfg.modifierId = DataManager::GetRandomModifierId();
                        }
                        droppedItems.push_back({ enemies[i].position, cfg });
                    }
                }
                enemies.erase(enemies.begin() + i);
            }
        }

        for (int i = (int)droppedItems.size() - 1; i >= 0; i--) {
            if (Vector3Distance(player->position, droppedItems[i].pos) < 1.0f) {
                if (player->AddToInventory(droppedItems[i].data)) {
                    logs.insert(logs.begin(), { "Picked up: " + Player::GetFullItemName(droppedItems[i].data), 4.0f, WHITE });
                    droppedItems.erase(droppedItems.begin() + i);
                }
            }
        }

        if (sceneTimer > 0) sceneTimer -= dt;
        else {
            if (Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) showPrompt = true;
            if (Vector3Distance(player->position, dungeon.stairsUpPos) < 2.0f) showPrompt = true;
            if (dungeon.portalPos.x != -999 && Vector3Distance(player->position, dungeon.portalPos) < 2.0f) {
                showPrompt = true;
            }
        }

        if (dungeon.healStationPos.x != -999 && Vector3Distance(player->position, dungeon.healStationPos) < 2.0f) {
            if (player->hp < player->maxHp) {
                player->hp += 50.0f * dt;
                if (player->hp > player->maxHp) player->hp = player->maxHp;
                if (GetRandomValue(0, 10) < 2) {
                    Vector3 p = Vector3Add(player->position, { (float)GetRandomValue(-5,5) / 10.0f, (float)GetRandomValue(0,20) / 10.0f, (float)GetRandomValue(-5,5) / 10.0f });
                    fxManager.SpawnEffect(p, { 0,1,0 }, FX_HIT, GREEN);
                }
            }
        }

        if (state == STATE_HOME && Vector3Distance(player->position, dungeon.storageBoxPos) < 2.0f && IsMouseButtonPressed(0)) showStorage = true;
        if (state == STATE_HOME && Vector3Distance(player->position, dungeon.reforgeStationPos) < 2.0f && IsMouseButtonPressed(0)) showReforgeMenu = true;
    }

    for (int i = (int)logs.size() - 1; i >= 0; i--) {
        logs[i].life -= dt;
        if (logs[i].life <= 0) logs.erase(logs.begin() + i);
    }
}

void Game::Draw() {
    BeginDrawing();
    ClearBackground(BLACK);

    BeginMode3D(camera);
    dungeon.Draw();
    fxManager.Draw();
    for (auto& item : droppedItems) {
        if (!debugMode && !dungeon.IsDiscovered(item.pos.x, item.pos.z)) continue;
        DrawCube(item.pos, 0.5f, 0.4f, 0.5f, YELLOW);
        DrawCubeWires(item.pos, 0.5f, 0.4f, 0.5f, ORANGE);
    }
    for (auto& e : enemies) if (debugMode || dungeon.IsDiscovered(e.position.x, e.position.z)) e.Draw(debugMode);
    player->Draw(debugMode);
    EndMode3D();

    fxManager.Draw2D(font, camera);
    UI::DrawHUD(*player, enemies, dungeon, camera, floor, debugMode, font);
    UI::DrawLogs(logs, font);

    // 【修正】UI::DrawNearbyItems 呼び出しに dungeon を追加
    UI::DrawNearbyItems(*player, droppedItems, dungeon, camera, font);

    if (showMenu) UI::DrawMenu(*player, dungeon, currentTab, font);
    if (showStorage) UI::DrawStorage(*player, font, showStorage, storageItems, storageEquip);
    if (showReforgeMenu) UI::DrawReforgeMenu(*player, font, showReforgeMenu);

    if (showPrompt) {
        const char* m = "UNKNOWN";
        if (floor == 0) m = "ENTER_DUNGEON";
        else if (Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) m = "GO_DEEPER";
        else if (Vector3Distance(player->position, dungeon.stairsUpPos) < 2.0f) m = "RETURN_HOME";
        else if (dungeon.portalPos.x != -999 && Vector3Distance(player->position, dungeon.portalPos) < 2.0f) m = "RETURN_HOME";

        int res = UI::DrawPrompt(m, screenWidth, screenHeight, font);
        if (res == 1) {
            if (Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) NextFloor();
            else ReturnHome();
            showPrompt = false; sceneTimer = 2.0f;
        }
        else if (res == 2) { showPrompt = false; sceneTimer = 1.0f; }
    }

    EndDrawing();
}

void Game::NextFloor() {
    floor++;
    state = STATE_DUNGEON;
    dungeon.Generate(false, floor);

    Vector3 startPos = dungeon.GetStartPosition();
    player->position = { startPos.x, 0.5f, startPos.z };

    droppedItems.clear();
    fxManager.projectiles.clear();
    fxManager.effects.clear();
    fxManager.damageTexts.clear();

    SpawnEnemies(10 + floor);

    std::vector<int> candidateItemIds;
    EnemyData currentEnemy = DataManager::GetRandomEnemyForFloor(floor);
    for (int id : currentEnemy.drops) candidateItemIds.push_back(id);
    EnemyData nextEnemy = DataManager::GetRandomEnemyForFloor(floor + 1);
    for (int id : nextEnemy.drops) candidateItemIds.push_back(id);

    for (const auto& pos : dungeon.treasureSpots) {
        ItemData item;
        if (!candidateItemIds.empty()) {
            int rndIdx = GetRandomValue(0, (int)candidateItemIds.size() - 1);
            item = DataManager::GetItemConfigCopy(candidateItemIds[rndIdx]);
        }
        if (item.id == -1 && !DataManager::itemConfigs.empty()) {
            int rndId = GetRandomValue(0, (int)DataManager::itemConfigs.size() - 1);
            item = DataManager::itemConfigs[rndId];
        }

        if (item.id != -1) {
            if (item.type == "EQUIP") {
                item.modifierId = DataManager::GetRandomModifierId();
            }
            droppedItems.push_back({ pos, item });
        }
    }
}

void Game::ReturnHome() {
    floor = 0;
    state = STATE_HOME;
    dungeon.Generate(true, 0);

    Vector3 startPos = dungeon.GetStartPosition();
    player->position = { startPos.x, 0.5f, startPos.z };

    enemies.clear();
    droppedItems.clear();
    fxManager.projectiles.clear();
    fxManager.effects.clear();
    fxManager.damageTexts.clear();
}