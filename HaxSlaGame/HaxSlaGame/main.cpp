#include "Dungeon.h"
#include "Player.h"
#include "Enemy.h"
#include "UI.h"
#include "DataManager.h"
#include "raymath.h"
#include <time.h>

int main() {
    const int sw = 1280, sh = 720; InitWindow(sw, sh, "3D Hack & Slash"); SetTargetFPS(60); srand((unsigned int)time(NULL)); EnableCursor();
    DataManager::LoadAllData(); Dungeon d; Player p(d.GetStartPosition());
    std::vector<Enemy> enemies; std::vector<DamageText> dt; std::vector<Projectile> ep; std::vector<DroppedItem> di;
    GameState state = STATE_HOME; int floor = 0; bool debug = false, menu = false, prompt = false; MenuTab tab = EQUIP; float sTimer = 0;
    auto Spawn = [&](int c) { enemies.clear(); if (state == STATE_HOME) return; for (int i = 0; i < c; i++) enemies.push_back(Enemy(d.GetRandomFloorPos(), DataManager::GetRandomEnemyForFloor(floor), floor)); };
    Camera3D cam = { {p.position.x + 15, 15, p.position.z + 15}, p.position, {0,1,0}, 45, 0 };
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_TAB)) menu = !menu;
        if (sTimer > 0) sTimer -= GetFrameTime();
        p.Update(cam, d, enemies, dt, menu || prompt);
        if (!menu && !prompt) {
            d.UpdateVisibility(p.position);
            if (sTimer <= 0) { if (Vector3Distance(p.position, d.stairsDownPos) < 1.5f) prompt = true; if (Vector3Distance(p.position, d.stairsUpPos) < 1.5f) prompt = true; }
            for (int i = (int)enemies.size() - 1; i >= 0; i--) {
                enemies[i].Update(p, d, ep);
                if (enemies[i].hp <= 0) {
                    p.AddExp(enemies[i].expValue, dt);
                    for (int id : enemies[i].data.drops) for (auto& cfg : DataManager::itemConfigs) if (cfg.id == id && ((float)GetRandomValue(0, 1000) / 1000.0f < cfg.dropChance)) di.push_back({ enemies[i].position, cfg });
                    enemies.erase(enemies.begin() + i);
                }
            }
            for (int i = (int)di.size() - 1; i >= 0; i--) if (Vector3Distance(p.position, di[i].pos) < 1.2f) { p.AddToInventory(di[i].data); di.erase(di.begin() + i); }
            for (int i = (int)ep.size() - 1; i >= 0; i--) { ep[i].pos = Vector3Add(ep[i].pos, Vector3Scale(ep[i].vel, GetFrameTime())); if (Vector3Distance(ep[i].pos, p.position) < (ep[i].radius + p.radius)) { p.hp -= fmaxf(1.0f, 10 + floor * 2 - p.defense); Vector3 kb = Vector3Scale(Vector3Normalize(ep[i].vel), 1.0f); if (!d.CheckCollisionRadius(Vector3Add(p.position, { kb.x,0,0 }), p.radius)) p.position.x += kb.x; if (!d.CheckCollisionRadius(Vector3Add(p.position, { 0,0,kb.z }), p.radius)) p.position.z += kb.z; ep.erase(ep.begin() + i); } else if (d.IsWall(ep[i].pos.x, ep[i].pos.z)) ep.erase(ep.begin() + i); }
            cam.target = p.position; cam.position = Vector3Add(p.position, Vector3Subtract(cam.position, cam.target));
            if (IsMouseButtonDown(1) || GetMouseWheelMove() != 0) UpdateCamera(&cam, CAMERA_THIRD_PERSON);
        }
        for (int i = (int)dt.size() - 1; i >= 0; i--) { dt[i].life -= GetFrameTime(); dt[i].pos.y += 0.05f; if (dt[i].life <= 0) dt.erase(dt.begin() + i); }
        BeginDrawing(); ClearBackground(BLACK); BeginMode3D(cam); d.Draw();
        for (auto& item : di) { DrawCube(item.pos, 0.5f, 0.1f, 0.5f, LIME); DrawCubeWires(item.pos, 0.5f, 0.1f, 0.5f, GREEN); }
        for (auto& e : enemies) if (debug || d.IsDiscovered(e.position.x, e.position.z)) e.Draw();
        for (auto& proj : ep) DrawSphere(proj.pos, proj.radius, RED);
        p.Draw(); EndMode3D(); UI::DrawHUD(p, enemies, d, cam, floor, debug); if (menu) UI::DrawMenu(p, d, tab);
        if (prompt) {
            const char* m = (floor == 0) ? "Enter Dungeon?" : (Vector3Distance(p.position, d.stairsDownPos) < 1.5f ? "Go Deeper?" : "Return Home?");
            int res = UI::DrawPrompt(m, sw, sh);
            if (res == 1) { if (Vector3Distance(p.position, d.stairsDownPos) < 1.5f) { floor++; state = STATE_DUNGEON; d.Generate(false); } else { floor = 0; state = STATE_HOME; d.Generate(true); } p.position = Vector3Add(d.GetStartPosition(), { 3,0,3 }); Spawn(10 + floor); prompt = false; sTimer = 2.0f; }
            else if (res == 2) { prompt = false; sTimer = 1.0f; }
        }
        for (auto& t : dt) { Vector2 s = GetWorldToScreen(t.pos, cam); if (t.amount == 999) DrawText("LEVEL UP!!", (int)s.x - 40, (int)s.y - 20, 26, YELLOW); else DrawText(TextFormat("%d", t.amount), (int)s.x - 10, (int)s.y, 22, YELLOW); }
        EndDrawing();
    }
    CloseWindow(); return 0;
}