#include "Dungeon.h"
#include "Player.h"
#include "Enemy.h"
#include "UI.h"
#include "DataManager.h"
#include "raymath.h"
#include <time.h>

int main() {
    const int sw = 1280; const int sh = 720; InitWindow(sw, sh, "3D Hack and Slash RPG"); SetTargetFPS(60); srand((unsigned int)time(NULL)); EnableCursor();
    DataManager::LoadAllData();
    std::vector<int> cps; for (int i = 32; i < 127; i++) cps.push_back(i); for (int i = 0x3000; i <= 0x30FF; i++) cps.push_back(i); for (int i = 0x4E00; i <= 0x9FAF; i++) cps.push_back(i);
    Font jpFont = LoadFontEx("jp_font.ttf", 32, cps.data(), (int)cps.size()); if (jpFont.texture.id == 0) jpFont = LoadFontEx("C:/Windows/Fonts/msgothic.ttc", 32, cps.data(), (int)cps.size());
    if (jpFont.texture.id == 0) jpFont = GetFontDefault(); else SetTextureFilter(jpFont.texture, TEXTURE_FILTER_BILINEAR);

    Dungeon d; Player p(d.GetStartPosition());
    std::vector<Enemy> enemies; std::vector<DamageText> dt; std::vector<Projectile> ep; std::vector<DroppedItem> di;
    std::vector<GameLog> logs; std::vector<ItemData> sItems, sEquip;
    GameState state = STATE_HOME; int floor = 0; bool debug = false, menu = false, prompt = false, storage = false; MenuTab tab = EQUIP; float sTimer = 0; int pType = 0;

    auto Spawn = [&](int c) { enemies.clear(); if (state == STATE_HOME) return; for (int i = 0; i < c; i++) enemies.push_back(Enemy(d.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(floor), floor)); };
    Camera3D cam = { Vector3{p.position.x + 15.0f, 15.0f, p.position.z + 15.0f}, p.position, Vector3{0.0f, 1.0f, 0.0f}, 45.0f, 0 };

    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_TAB)) menu = !menu;
        if (IsKeyPressed(KEY_F1)) debug = !debug;
        if (debug) {
            if (IsKeyPressed(KEY_N)) { floor++; state = STATE_DUNGEON; d.Generate(false); p.position = Vector3Add(d.GetStartPosition(), Vector3{ 3,0,3 }); Spawn(10 + floor); sTimer = 2.0f; }
            if (IsKeyPressed(KEY_E)) enemies.push_back(Enemy(d.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(floor), floor));
            if (IsKeyPressed(KEY_R)) { floor = 0; state = STATE_HOME; d.Generate(true); p.position = d.GetStartPosition(); Spawn(0); }
        }
        if (sTimer > 0) sTimer -= GetFrameTime();
        Vector3 offset = Vector3Subtract(cam.position, cam.target);
        p.Update(cam, d, enemies, dt, menu || prompt || storage);
        if (!menu && !prompt && !storage) {
            d.UpdateVisibility(p.position);
            if (sTimer <= 0) { if (Vector3Distance(p.position, d.stairsDownPos) < 1.5f) { prompt = true; pType = 0; } if (Vector3Distance(p.position, d.stairsUpPos) < 1.5f) { prompt = true; pType = 1; } }
            if (state == STATE_HOME && Vector3Distance(p.position, d.storageBoxPos) < 2.0f && IsMouseButtonPressed(0)) storage = true;
            for (int i = (int)enemies.size() - 1; i >= 0; i--) {
                enemies[i].Update(p, d, ep);
                if (enemies[i].hp <= 0) {
                    p.AddExp(enemies[i].expValue, dt);
                    for (int id : enemies[i].data.drops) for (auto& cfg : DataManager::itemConfigs) if (cfg.id == id && ((float)GetRandomValue(0, 1000) / 1000.0f < cfg.dropChance)) di.push_back(DroppedItem{ enemies[i].position, cfg });
                    enemies.erase(enemies.begin() + i);
                }
            }
            for (int i = (int)di.size() - 1; i >= 0; i--) if (Vector3Distance(p.position, di[i].pos) < 1.2f) { if (p.AddToInventory(di[i].data)) { logs.insert(logs.begin(), GameLog{ "Picked up: " + di[i].data.name, 4.0f, WHITE }); di.erase(di.begin() + i); } else if (logs.empty() || logs[0].message.find("Full") == std::string::npos) logs.insert(logs.begin(), GameLog{ "Inventory Full!", 2.0f, RED }); }
            for (int i = (int)ep.size() - 1; i >= 0; i--) { ep[i].pos = Vector3Add(ep[i].pos, Vector3Scale(ep[i].vel, GetFrameTime())); if (Vector3Distance(ep[i].pos, p.position) < (ep[i].radius + p.radius)) { p.hp -= fmaxf(1.0f, 10 + floor * 2 - p.defense); Vector3 kb = Vector3Scale(Vector3Normalize(ep[i].vel), 1.0f); if (!d.CheckCollisionRadius(Vector3Add(p.position, Vector3{ kb.x,0,0 }), p.radius)) p.position.x += kb.x; if (!d.CheckCollisionRadius(Vector3Add(p.position, Vector3{ 0,0,kb.z }), p.radius)) p.position.z += kb.z; ep.erase(ep.begin() + i); } else if (d.IsWall(ep[i].pos.x, ep[i].pos.z)) ep.erase(ep.begin() + i); }
            cam.target = p.position; cam.position = Vector3Add(p.position, offset);
            if (IsMouseButtonDown(1) || GetMouseWheelMove() != 0) UpdateCamera(&cam, CAMERA_THIRD_PERSON);
        }
        for (auto& log : logs) log.life -= GetFrameTime();
        for (int i = (int)logs.size() - 1; i >= 0; i--) if (logs[i].life <= 0) logs.erase(logs.begin() + i);
        if (logs.size() > 8) logs.pop_back();
        for (int i = (int)dt.size() - 1; i >= 0; i--) { dt[i].life -= GetFrameTime(); dt[i].pos.y += 0.05f; if (dt[i].life <= 0) dt.erase(dt.begin() + i); }

        BeginDrawing();
        ClearBackground(BLACK); BeginMode3D(cam); d.Draw();
        for (auto& item : di) { DrawCube(item.pos, 0.5f, 0.1f, 0.5f, LIME); DrawCubeWires(item.pos, 0.5f, 0.1f, 0.5f, GREEN); }
        for (auto& e : enemies) if (debug || d.IsDiscovered(e.position.x, e.position.z)) e.Draw();
        for (auto& proj : ep) DrawSphere(proj.pos, proj.radius, RED);
        p.Draw(); EndMode3D();
        UI::DrawHUD(p, enemies, d, cam, floor, debug, jpFont); UI::DrawLogs(logs, jpFont); UI::DrawNearbyItems(p, di, cam, jpFont);
        if (menu) UI::DrawMenu(p, d, tab, jpFont); if (storage) UI::DrawStorage(p, jpFont, storage, sItems, sEquip);
        if (prompt) {
            const char* m = (floor == 0) ? "ENTER_DUNGEON" : (Vector3Distance(p.position, d.stairsDownPos) < 1.5f ? "GO_DEEPER" : "RETURN_HOME");
            int res = UI::DrawPrompt(m, sw, sh, jpFont);
            if (res == 1) { if (Vector3Distance(p.position, d.stairsDownPos) < 1.5f) { floor++; state = STATE_DUNGEON; d.Generate(false); } else { floor = 0; state = STATE_HOME; d.Generate(true); } p.position = Vector3Add(d.GetStartPosition(), Vector3{ 3.0f,0.0f,3.0f }); Spawn(10 + floor); prompt = false; sTimer = 2.0f; }
            else if (res == 2) { prompt = false; sTimer = 1.0f; }
        }
        for (auto& t : dt) { Vector2 s = GetWorldToScreen(t.pos, cam); if (t.amount == 999) DrawTextEx(jpFont, DataManager::uiStrings["LEVEL_UP"].c_str(), Vector2{ s.x - 50.0f, s.y - 30.0f }, 28.0f, 1.0f, YELLOW); else if (t.amount > 0) DrawTextEx(jpFont, TextFormat("%d", t.amount), Vector2{ s.x - 10.0f, s.y }, 22.0f, 1.0f, ORANGE); }
        EndDrawing();
    }
    UnloadFont(jpFont); CloseWindow(); return 0;
}