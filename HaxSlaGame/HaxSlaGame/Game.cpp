#include "Game.h"
#include "DataManager.h"
#include "raymath.h"
#include <iostream>
#include <time.h>

Game::Game() {
    InitWindow(screenWidth, screenHeight, "3D Hack and Slash RPG Refactored");
    SetTargetFPS(60);
    DataManager::LoadAllData();

    // 日本語フォントロード (適宜パス調整)
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
    dungeon.Generate(true);

    if (player) delete player;
    player = new Player(dungeon.GetStartPosition());

    camera = { {player->position.x + 10.0f, 15.0f, player->position.z + 10.0f}, player->position, {0.0f, 1.0f, 0.0f}, 45.0f, 0 };

    fxManager.projectiles.clear();
    fxManager.effects.clear();
    fxManager.damageTexts.clear();
    enemies.clear();
    droppedItems.clear();

    debugMode = false;
    showMenu = false;
    showStorage = false;
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

    // 入力・デバッグ
    if (IsKeyPressed(KEY_F1)) debugMode = !debugMode;
    if (IsKeyPressed(KEY_TAB)) showMenu = !showMenu;
    if (debugMode) {
        if (IsKeyPressed(KEY_N)) NextFloor();
        if (IsKeyPressed(KEY_R)) ReturnHome();
    }

    bool stopPlayer = showMenu || showPrompt || showStorage;

    // カメラ
    Vector3 offset = Vector3Subtract(camera.position, camera.target);
    camera.target = player->position;
    camera.position = Vector3Add(player->position, offset);
    if (!stopPlayer) {
        if (IsMouseButtonDown(1) || GetMouseWheelMove() != 0) UpdateCamera(&camera, CAMERA_THIRD_PERSON);
    }

    // 更新
    player->Update(camera, dungeon, enemies, fxManager, stopPlayer);
    fxManager.Update(dt, dungeon);
    fxManager.CheckProjectileCollisions(enemies, *player, dungeon);
    dungeon.UpdateVisibility(player->position);

    if (!stopPlayer) {
        // 敵AI
        for (int i = (int)enemies.size() - 1; i >= 0; i--) {
            enemies[i].Update(*player, dungeon, fxManager);
            if (enemies[i].hp <= 0) {
                player->AddExp(enemies[i].expValue, fxManager);
                // アイテムドロップ
                for (int id : enemies[i].data.drops) {
                    ItemData cfg = DataManager::GetItemConfigCopy(id);
                    if (cfg.id != -1 && (float)GetRandomValue(0, 1000) / 1000.0f < cfg.dropChance)
                        droppedItems.push_back({ enemies[i].position, cfg });
                }
                enemies.erase(enemies.begin() + i);
            }
        }

        // アイテム拾う
        for (int i = (int)droppedItems.size() - 1; i >= 0; i--) {
            if (Vector3Distance(player->position, droppedItems[i].pos) < 1.2f && IsMouseButtonPressed(0)) {
                if (player->AddToInventory(droppedItems[i].data)) {
                    logs.insert(logs.begin(), { "Picked up: " + droppedItems[i].data.name, 4.0f, WHITE });
                    droppedItems.erase(droppedItems.begin() + i);
                }
            }
        }

        // シーン遷移
        if (sceneTimer > 0) sceneTimer -= dt;
        else {
            if (Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f) showPrompt = true;
            if (Vector3Distance(player->position, dungeon.stairsUpPos) < 2.0f) showPrompt = true;
        }
        if (state == STATE_HOME && Vector3Distance(player->position, dungeon.storageBoxPos) < 2.0f && IsMouseButtonPressed(0)) showStorage = true;
    }

    // ログ消失
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
        DrawCube(item.pos, 0.5f, 0.1f, 0.5f, LIME);
        DrawCubeWires(item.pos, 0.5f, 0.1f, 0.5f, GREEN);
    }
    for (auto& e : enemies) if (debugMode || dungeon.IsDiscovered(e.position.x, e.position.z)) e.Draw(debugMode);
    player->Draw(debugMode);
    EndMode3D();

    // 2D UIレイヤー
    fxManager.Draw2D(font, camera);
    UI::DrawHUD(*player, enemies, dungeon, camera, floor, debugMode, font);
    UI::DrawLogs(logs, font);
    UI::DrawNearbyItems(*player, droppedItems, camera, font);

    if (showMenu) UI::DrawMenu(*player, dungeon, currentTab, font);
    if (showStorage) UI::DrawStorage(*player, font, showStorage, storageItems, storageEquip);

    if (showPrompt) {
        const char* m = (floor == 0) ? "ENTER_DUNGEON" : (Vector3Distance(player->position, dungeon.stairsDownPos) < 2.0f ? "GO_DEEPER" : "RETURN_HOME");
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
    dungeon.Generate(false);
    player->position = Vector3Add(dungeon.GetStartPosition(), { 3,0,3 });
    SpawnEnemies(10 + floor);
    fxManager.projectiles.clear();
}

void Game::ReturnHome() {
    floor = 0;
    state = STATE_HOME;
    dungeon.Generate(true);
    player->position = dungeon.GetStartPosition();
    SpawnEnemies(0);
    fxManager.projectiles.clear();
}