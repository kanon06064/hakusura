#include "Game.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "raymath.h"
#include "rlgl.h"
#include <iostream>
#include <time.h>
#include <algorithm>

static std::string T(const std::string& key, const std::string& def) {
    if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
    return def;
}

Game::Game()
    : screenWidth(1280), screenHeight(720), player(nullptr), camera({ 0 }), state(STATE_TITLE),
    floor(0), currentDungeonId(0), unlockedDungeonId(0), currentSlot(0), hoveredEntranceIndex(-1),
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
    hoveredEntranceIndex = -1;

    state = STATE_HOME; bossDefeated = false;
    dungeon.Generate(true, 0, currentDungeonId, unlockedDungeonId);

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
        if (i.type == "EQUIP" || i.type == "ARMOR") { i.modifierId = 7; player->inventoryEquip.push_back(i); }
        else { i.count = 99; player->inventoryItems.push_back(i); }
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
    player->RecalculateStats(); player->hp = player->maxHp;

    unlockedDungeonId = 2; maxFloors = { 30, 50, 100 };
    SaveCurrentSlot();
    logs.insert(logs.begin(), { T("LOG_RUSH_START", "RUSH MODE STARTED!"), 5.0f, ORANGE });
}

void Game::InitDebugRoom() {
    floor = -1; state = STATE_DEBUG_ROOM; enemies.clear();
    camera.position = { 10.0f, 10.0f, 20.0f }; camera.target = { 10.0f, 0.0f, 0.0f }; camera.up = { 0.0f, 1.0f, 0.0f }; camera.fovy = 45.0f; camera.projection = CAMERA_PERSPECTIVE;
    int x = 0;
    for (const auto& data : DataManager::allEnemyData) {
        for (int i = 0; i < 4; i++) {
            Vector3 pos = { (float)x * 2.5f, 0.5f, (float)i * 2.5f };
            Enemy e(pos, data, 1);
            if (i == 0) e.state = STATE_PATROL; if (i == 1) e.state = STATE_CHASE; if (i == 2) e.state = STATE_ATTACK; if (i == 3) e.StartDeath();
            enemies.push_back(e);
        }
        x++;
    }
}

void Game::StartDebugRoom() { if (player) delete player; player = new Player({ 0,0,0 }); InitDebugRoom(); }

void Game::LoadAndStart(int slot) {
    currentSlot = slot; InitGame();
    if (DataManager::LoadGame(slot, player, floor, currentDungeonId, unlockedDungeonId, maxFloors, storageItems, storageEquip, isPortfolioMode)) {
        if (floor == 0) { state = STATE_HOME; dungeon.Generate(true, 0, currentDungeonId, unlockedDungeonId); AudioManager::PlayBGM(BGM_HOME); }
        else { state = STATE_DUNGEON; dungeon.Generate(false, floor, currentDungeonId, unlockedDungeonId); SpawnEnemies(10 + floor); AudioManager::PlayBGM(BGM_DUNGEON); }
        Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    }
    else { InitGame(); }
}

void Game::NewGameAndStart(int slot) { currentSlot = slot; InitGame(); }

void Game::SaveCurrentSlot() {
    if (player) { DataManager::SaveGame(currentSlot, player, floor, currentDungeonId, unlockedDungeonId, maxFloors, storageItems, storageEquip, isPortfolioMode); AudioManager::PlaySE(SE_SAVE); }
}

void Game::SpawnEnemies(int count) {
    if (state == STATE_HOME) return; enemies.clear();
    if (isPortfolioMode) {
        if (floor == 1) { for (int i = 0; i < count; i++) { enemies.push_back(Enemy(dungeon.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(30, 0), 30)); } }
        else if (floor == 2) { if (dungeon.bossSpawnPos.x != -999) { Enemy boss(dungeon.bossSpawnPos, DataManager::GetBossEnemy(30, 0), 50); boss.maxHp *= 3.0f; boss.hp = boss.maxHp; boss.radius = 1.5f; boss.isBoss = true; enemies.push_back(boss); } bossDefeated = false; }
        else if (floor == 3) { if (dungeon.bossSpawnPos.x != -999) { Enemy boss(dungeon.bossSpawnPos, DataManager::GetBossEnemy(100, 2), 100); boss.maxHp *= 3.0f; boss.hp = boss.maxHp; boss.radius = 1.5f; boss.isBoss = true; enemies.push_back(boss); } bossDefeated = false; }
        return;
    }
    if (floor > 0 && floor % 10 == 5) return;
    int enemyLevel = floor; if (currentDungeonId == 1) enemyLevel += 30; if (currentDungeonId == 2) enemyLevel += 60;
    if (floor > 0 && floor % 10 == 0) {
        if (dungeon.bossSpawnPos.x != -999) { Enemy boss(dungeon.bossSpawnPos, DataManager::GetBossEnemy(floor, currentDungeonId), enemyLevel); boss.maxHp *= 3.0f; boss.hp = boss.maxHp; boss.radius = 1.5f; boss.isBoss = true; enemies.push_back(boss); }
        bossDefeated = false; return;
    }
    for (int i = 0; i < count; i++) { enemies.push_back(Enemy(dungeon.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(floor, currentDungeonId), enemyLevel)); }
}

void Game::Run() { while (!WindowShouldClose()) { Update(); Draw(); } }

void Game::UpdateDebugRoom() {
    AudioManager::Update(); UpdateCamera(&camera, CAMERA_FREE);
    for (auto& e : enemies) e.animFrameCounter++;
    if (IsKeyPressed(KEY_ESCAPE) || IsKeyPressed(KEY_TAB) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) { state = STATE_TITLE; enemies.clear(); AudioManager::PlayBGM(BGM_TITLE); }
}

void Game::Update() {
    AudioManager::Update();

    bool stopPlayer = showMenu || showPrompt || showStorage || showReforgeMenu || showWarpMenu || showCraftMenu || showQuestMenu || state == STATE_TITLE;
    if (UI::showDetail) stopPlayer = true;

    // ★追加: 前フレームで登録されたボタンに対して、十字キーでマウス移動を行う
    if (stopPlayer) {
        UI::UpdatePadNavigation();
    }
    UI::ClearInteractables(); // 一旦クリア（次の描画で再登録）

    // ★追加: 「戻る/キャンセル」ボタンの共通処理
    bool cancelInput = false;
    if (IsGamepadAvailable(0)) {
        if (stopPlayer || state == STATE_TITLE) {
            if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)) cancelInput = true;
        }
    }
    if (IsKeyPressed(KEY_ESCAPE) || (stopPlayer && IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))) {
        cancelInput = true;
    }

    if (cancelInput) {
        AudioManager::PlaySE(SE_CLICK);
        if (UI::showDetail) UI::showDetail = false;
        else if (UI::deleteConfirmSlot > 0) UI::deleteConfirmSlot = 0;
        else if (showPrompt) { showPrompt = false; sceneTimer = 1.0f; }
        else if (showMenu) showMenu = false;
        else if (showStorage) showStorage = false;
        else if (showReforgeMenu) showReforgeMenu = false;
        else if (showWarpMenu) showWarpMenu = false;
        else if (showCraftMenu) showCraftMenu = false;
        else if (showQuestMenu) showQuestMenu = false;
    }

    if (state == STATE_TITLE) return;
    if (state == STATE_DEBUG_ROOM) { UpdateDebugRoom(); return; }

    float dt = GetFrameTime();
    if (state == STATE_GAMEOVER || state == STATE_GAMECLEAR) {
        if (IsMouseButtonPressed(0) || IsKeyPressed(KEY_SPACE) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN)) {
            ApplyDeathPenalty();
        }
        return;
    }

    if (IsKeyPressed(KEY_F1)) debugMode = !debugMode;
    if (IsKeyPressed(KEY_TAB) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_MIDDLE_RIGHT)) {
        if (!UI::showDetail) { showMenu = !showMenu; AudioManager::PlaySE(SE_CLICK); }
    }
    if (debugMode) { if (IsKeyPressed(KEY_N)) NextFloor(); if (IsKeyPressed(KEY_R)) ReturnHome(); }

    if (IsGamepadAvailable(0) && stopPlayer) {
        float mx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float my = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabs(mx) > 0.1f || fabs(my) > 0.1f) {
            Vector2 mousePos = GetMousePosition();
            mousePos.x += mx * 12.0f;
            mousePos.y += my * 12.0f;
            SetMousePosition((int)mousePos.x, (int)mousePos.y);
        }
    }

    Vector3 camOffset = Vector3Subtract(camera.position, camera.target);
    float dist = Vector3Length(camOffset);
    float horizontalDist = sqrtf(camOffset.x * camOffset.x + camOffset.z * camOffset.z);
    float pitch = atan2f(camOffset.y, horizontalDist);
    float yaw = atan2f(camOffset.x, camOffset.z);

    if (!stopPlayer) {
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            yaw -= delta.x * 0.01f;
            pitch -= delta.y * 0.01f; // ★修正: マウスの上下操作も反転 (- に変更)
        }
        if (IsGamepadAvailable(0)) {
            float rx = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X);
            float ry = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y);
            if (fabs(rx) > 0.2f || fabs(ry) > 0.2f) {
                yaw -= rx * 0.05f;
                pitch -= ry * 0.05f; // ★修正: スティック上下の反転 (- に変更)
            }
        }
        float wheel = GetMouseWheelMove();
        if (wheel != 0) {
            dist -= wheel * 2.0f;
        }
    }

    float minPitch = 10.0f * DEG2RAD; float maxPitch = 80.0f * DEG2RAD;
    if (pitch < minPitch) pitch = minPitch;
    if (pitch > maxPitch) pitch = maxPitch;
    float minDist = 5.0f; float maxDist = 30.0f;
    if (dist < minDist) dist = minDist;
    if (dist > maxDist) dist = maxDist;

    camera.target = player->position;
    float newHorizontal = dist * cosf(pitch);
    camOffset.y = dist * sinf(pitch);
    camOffset.x = newHorizontal * sinf(yaw);
    camOffset.z = newHorizontal * cosf(yaw);
    camera.position = Vector3Add(camera.target, camOffset);

    player->Update(camera, dungeon, enemies, fxManager, stopPlayer);
    fxManager.Update(dt, dungeon); fxManager.CheckProjectileCollisions(enemies, *player, dungeon); dungeon.UpdateVisibility(player->position);

    if (!stopPlayer) {
        if (player->hp <= 0 && state != STATE_HOME) { state = STATE_GAMEOVER; AudioManager::PlayBGM(BGM_NONE); }

        for (int i = (int)enemies.size() - 1; i >= 0; i--) {
            enemies[i].Update(*player, dungeon, fxManager);
            if (enemies[i].hp <= 0 && !enemies[i].isDying) {
                player->AddExp(enemies[i].expValue, fxManager); player->gold += enemies[i].data.gold;
                player->UpdateHuntQuest(enemies[i].data.id);
                for (int id : enemies[i].data.drops) {
                    ItemData cfg = DataManager::GetItemConfigCopy(id);
                    if (cfg.id != -1) {
                        float chance = cfg.dropChance;
                        if ((float)GetRandomValue(0, 10000) / 10000.0f <= chance) {
                            if (cfg.type == "EQUIP" || cfg.type == "ARMOR") cfg.modifierId = DataManager::GetRandomModifierId();
                            DroppedItem di; di.pos = enemies[i].position; di.pos.y += 0.5f; di.data = cfg;
                            di.vel = { (float)GetRandomValue(-15, 15) * 0.01f, (float)GetRandomValue(30, 40) * 0.01f, (float)GetRandomValue(-15, 15) * 0.01f };
                            di.rotation = (float)GetRandomValue(0, 360);
                            droppedItems.push_back(di);
                        }
                    }
                }
                enemies[i].StartDeath();
            }
            if (enemies[i].isDead) { enemies.erase(enemies.begin() + i); }
        }

        for (auto& item : droppedItems) {
            Vector3 nextX = item.pos; nextX.x += item.vel.x;
            if (!dungeon.CheckCollisionRadius(nextX, 0.2f)) { item.pos.x = nextX.x; }
            else { item.vel.x *= -0.8f; }
            Vector3 nextZ = item.pos; nextZ.z += item.vel.z;
            if (!dungeon.CheckCollisionRadius(nextZ, 0.2f)) { item.pos.z = nextZ.z; }
            else { item.vel.z *= -0.8f; }

            item.pos.y += item.vel.y; item.vel.y -= 2.0f * dt;
            if (item.pos.y <= 0.2f) {
                item.pos.y = 0.2f; item.vel.y *= -0.5f; item.vel.x *= 0.8f; item.vel.z *= 0.8f;
                if (fabsf(item.vel.y) < 0.05f) item.vel.y = 0;
            }
            item.rotation += 100.0f * dt * (fabsf(item.vel.x) + fabsf(item.vel.z));
        }

        if (isPortfolioMode) {
            if ((floor == 2 || floor == 3) && enemies.empty() && !bossDefeated) {
                bossDefeated = true; logs.insert(logs.begin(), { T("LOG_BOSS_DEFEAT", "BOSS DEFEATED!"), 5.0f, GOLD });
                if (floor == 3) { state = STATE_GAMECLEAR; AudioManager::PlayBGM(BGM_NONE); }
            }
        }
        else {
            if (floor > 0 && floor % 10 == 0 && enemies.empty() && !bossDefeated) {
                bossDefeated = true;
                int maxF = (currentDungeonId == 0) ? 30 : (currentDungeonId == 1) ? 50 : 100;
                if (floor == maxF) {
                    if (currentDungeonId == unlockedDungeonId && unlockedDungeonId < 2) {
                        unlockedDungeonId++; UI::AddSystemLog(T("LOG_NEXT_DUNGEON", "NEXT DUNGEON UNLOCKED!"), LIME);
                    }
                    if (currentDungeonId == 2) { state = STATE_GAMECLEAR; AudioManager::PlayBGM(BGM_NONE); }
                    else { UI::AddSystemLog(T("LOG_DUNGEON_CLEAR", "DUNGEON CLEARED!"), GOLD); }
                }
                else { UI::AddSystemLog(T("LOG_BOSS_DEFEAT", "BOSS DEFEATED!"), GOLD); }
            }
        }

        for (int i = (int)droppedItems.size() - 1; i >= 0; i--) {
            if (Vector3Distance(player->position, droppedItems[i].pos) < 1.0f) {
                if (player->AddToInventory(droppedItems[i].data)) {
                    UI::AddSystemLog(TextFormat(T("LOG_ITEM_FOUND", "Found: %s").c_str(), Player::GetFullItemName(droppedItems[i].data).c_str()), Player::GetItemRarityColor(droppedItems[i].data));
                    droppedItems.erase(droppedItems.begin() + i); AudioManager::PlaySE(SE_CLICK);
                }
            }
        }

        auto dist2D = [](Vector3 a, Vector3 b) { return Vector2Distance({ a.x, a.z }, { b.x, b.z }); };
        bool clickAction = IsMouseButtonPressed(0) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);

        if (sceneTimer > 0) sceneTimer -= dt;
        else {
            bool canUseStairs = true;
            if (!isPortfolioMode && floor > 0 && floor % 10 == 0 && !bossDefeated) canUseStairs = false;
            if (isPortfolioMode && (floor == 2 || floor == 3) && !bossDefeated) canUseStairs = false;

            if (state == STATE_HOME) {
                hoveredEntranceIndex = -1;
                for (int i = 0; i < 3; i++) {
                    if (dungeon.dungeonEntrances[i].x != -999 && dist2D(player->position, dungeon.dungeonEntrances[i]) < 2.0f) {
                        hoveredEntranceIndex = i; showPrompt = true;
                    }
                }
            }
            if (state == STATE_DUNGEON) {
                if (canUseStairs && dungeon.stairsDownPos.x != -999 && dist2D(player->position, dungeon.stairsDownPos) < 2.0f) { showPrompt = true; }
                if (dungeon.stairsUpPos.x != -999 && dist2D(player->position, dungeon.stairsUpPos) < 2.0f) showPrompt = true;
                if (dungeon.portalPos.x != -999 && dist2D(player->position, dungeon.portalPos) < 2.0f) showPrompt = true;
            }
        }

        if (dungeon.healStationPos.x != -999 && dist2D(player->position, dungeon.healStationPos) < 2.0f) {
            if (player->hp < player->maxHp) { player->hp += 50.0f * dt; if (player->hp > player->maxHp) player->hp = player->maxHp; if (GetRandomValue(0, 10) < 2) fxManager.SpawnEffect(Vector3Add(player->position, { 0,1,0 }), { 0,1,0 }, FX_HIT, GREEN); }
        }

        if (state == STATE_HOME) {
            if (dungeon.storageBoxPos.x != -999 && dist2D(player->position, dungeon.storageBoxPos) < 2.0f && clickAction) { showStorage = true; AudioManager::PlaySE(SE_CLICK); }
            if (dungeon.reforgeStationPos.x != -999 && dist2D(player->position, dungeon.reforgeStationPos) < 2.0f && clickAction) { showReforgeMenu = true; AudioManager::PlaySE(SE_CLICK); }
            if (dungeon.portalPos.x != -999 && dist2D(player->position, dungeon.portalPos) < 2.0f && clickAction) { showWarpMenu = true; AudioManager::PlaySE(SE_CLICK); }
            if (dungeon.craftStationPos.x != -999 && dist2D(player->position, dungeon.craftStationPos) < 2.0f && clickAction) { showCraftMenu = true; AudioManager::PlaySE(SE_CLICK); }
            if (dungeon.questBoardPos.x != -999 && dist2D(player->position, dungeon.questBoardPos) < 2.0f && clickAction) { showQuestMenu = true; AudioManager::PlaySE(SE_CLICK); }
        }
    }
    for (int i = (int)logs.size() - 1; i >= 0; i--) { logs[i].life -= dt; if (logs[i].life <= 0) logs.erase(logs.begin() + i); }
}

void Game::ApplyDeathPenalty() {
    if (isPortfolioMode) {
        state = STATE_TITLE; isPortfolioMode = false; AudioManager::PlayBGM(BGM_TITLE); return;
    }
    if (state == STATE_GAMEOVER) {
        player->inventoryItems.clear(); logs.clear();
        UI::AddSystemLog(T("LOG_RETURN_HOME", "Returned to Home..."), WHITE);
        UI::AddSystemLog(T("LOG_LOST_MATS", "Lost materials."), RED);
    }
    else {
        logs.clear();
        UI::AddSystemLog(T("LOG_CONGRATS", "CONGRATULATIONS!"), GOLD);
        UI::AddSystemLog(T("LOG_RETURN_HOME", "Returned to Home."), WHITE);
    }
    player->hp = player->maxHp; ReturnHome();
}

void Game::NextFloor() {
    if (isPortfolioMode) {
        floor++; state = STATE_DUNGEON; bossDefeated = false;
        if (floor == 1) { dungeon.Generate(false, 1, currentDungeonId, unlockedDungeonId); SpawnEnemies(15); }
        else if (floor == 2) { dungeon.Generate(false, 10, currentDungeonId, unlockedDungeonId); SpawnEnemies(0); }
        else if (floor == 3) { dungeon.Generate(false, 100, currentDungeonId, unlockedDungeonId); SpawnEnemies(0); }
        Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
        droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();
        AudioManager::PlayBGM(BGM_DUNGEON); return;
    }

    floor++; if (floor > maxFloors[currentDungeonId]) maxFloors[currentDungeonId] = floor;
    state = STATE_DUNGEON; bossDefeated = false;
    dungeon.Generate(false, floor, currentDungeonId, unlockedDungeonId);
    Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();

    int enemyLevel = floor; if (currentDungeonId == 1) enemyLevel += 30; if (currentDungeonId == 2) enemyLevel += 60;
    SpawnEnemies(10 + floor); AudioManager::PlayBGM(BGM_DUNGEON);

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
                DroppedItem di; di.pos = pos; di.pos.y = 0.2f; di.vel = { 0,0,0 }; di.rotation = (float)GetRandomValue(0, 360); di.data = item;
                droppedItems.push_back(di);
            }
        }
    }
}

void Game::WarpToFloor(int targetDungeon, int targetFloor) {
    currentDungeonId = targetDungeon; floor = targetFloor; state = STATE_DUNGEON; bossDefeated = false;
    dungeon.Generate(false, floor, currentDungeonId, unlockedDungeonId);
    Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();

    int enemyLevel = floor; if (currentDungeonId == 1) enemyLevel += 30; if (currentDungeonId == 2) enemyLevel += 60;
    SpawnEnemies(10 + floor); showWarpMenu = false; AudioManager::PlayBGM(BGM_DUNGEON); AudioManager::PlaySE(SE_STAIRS);
}

void Game::ReturnHome() {
    floor = 0; state = STATE_HOME; bossDefeated = false;
    dungeon.Generate(true, 0, currentDungeonId, unlockedDungeonId);
    Vector3 startPos = dungeon.GetStartPosition(); player->position = { startPos.x, 0.5f, startPos.z };
    enemies.clear(); droppedItems.clear(); fxManager.projectiles.clear(); fxManager.effects.clear(); fxManager.damageTexts.clear();
    AudioManager::PlayBGM(BGM_HOME); AudioManager::PlaySE(SE_STAIRS);
}