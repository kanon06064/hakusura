#include "raylib.h"
#include "raymath.h"
#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include <vector>
#include <time.h>

int main() {
    const int screenWidth = 1280, screenHeight = 720;
    InitWindow(screenWidth, screenHeight, "3D Hack & Slash - Alpha");
    SetTargetFPS(60); srand((unsigned int)time(NULL));

    Dungeon dungeon; Player player(dungeon.GetStartPosition());
    std::vector<Enemy> enemies; std::vector<DamageText> dmgTexts; std::vector<Projectile> enemyProj;
    bool debugMode = false;

    auto Spawn = [&](int count) {
        for (int i = 0; i < count; i++) {
            Vector3 p; do { p = dungeon.GetRandomFloorPos(); } while (Vector3Distance(p, player.position) < 15.0f);
            enemies.push_back(Enemy(p, (EnemyType)GetRandomValue(0, 5)));
        }
        };
    Spawn(10);

    Camera3D camera = { {player.position.x + 15, 15, player.position.z + 15}, player.position, {0,1,0}, 45.0f, CAMERA_PERSPECTIVE };

    while (!WindowShouldClose()) {
        Vector3 cameraOffset = Vector3Subtract(camera.position, camera.target);
        player.Update(camera, dungeon, enemies, dmgTexts);
        dungeon.UpdateVisibility(player.position);

        for (int i = (int)enemies.size() - 1; i >= 0; i--) {
            enemies[i].Update(player, dungeon, enemyProj);
            if (enemies[i].hp <= 0) enemies.erase(enemies.begin() + i);
        }
        for (int i = (int)enemyProj.size() - 1; i >= 0; i--) {
            enemyProj[i].pos = Vector3Add(enemyProj[i].pos, Vector3Scale(enemyProj[i].vel, GetFrameTime()));
            if (Vector3Distance(enemyProj[i].pos, player.position) < (enemyProj[i].radius + player.radius)) {
                player.hp -= 5.0f; enemyProj.erase(enemyProj.begin() + i);
            }
            else if (dungeon.IsWall(enemyProj[i].pos.x, enemyProj[i].pos.z)) enemyProj.erase(enemyProj.begin() + i);
        }
        for (int i = (int)dmgTexts.size() - 1; i >= 0; i--) {
            dmgTexts[i].life -= GetFrameTime(); dmgTexts[i].pos.y += 0.05f;
            if (dmgTexts[i].life <= 0) dmgTexts.erase(dmgTexts.begin() + i);
        }

        camera.target = player.position; camera.position = Vector3Add(camera.target, cameraOffset);
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || GetMouseWheelMove() != 0) UpdateCamera(&camera, CAMERA_THIRD_PERSON);

        if (IsKeyPressed(KEY_F1)) debugMode = !debugMode;
        if (IsKeyPressed(KEY_E)) Spawn(1);
        if (IsKeyPressed(KEY_R)) { dungeon.Generate(); player.position = dungeon.GetStartPosition(); enemies.clear(); enemyProj.clear(); dmgTexts.clear(); Spawn(10); }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        dungeon.Draw();
        for (auto& e : enemies) if (debugMode || dungeon.IsDiscovered(e.position.x, e.position.z)) e.Draw();
        for (auto& p : enemyProj) DrawSphere(p.pos, p.radius, RED);
        player.Draw();
        EndMode3D();

        // HPバー・HUD描画（既存のものを維持）
        for (auto& e : enemies) if (debugMode || dungeon.IsDiscovered(e.position.x, e.position.z)) {
            Vector2 sPos = GetWorldToScreen({ e.position.x, 1.8f, e.position.z }, camera);
            DrawRectangle((int)sPos.x - 20, (int)sPos.y, 40, 4, DARKGRAY);
            DrawRectangle((int)sPos.x - 20, (int)sPos.y, (int)(40 * (e.hp / e.maxHp)), 4, RED);
        }
        int hudX = screenWidth - 260, tCount = 0;
        const char* eNames[] = { "SWORD", "SPEAR", "AXE", "ARCHER", "MAGE", "TRAP" };
        for (auto& e : enemies) if (e.hudTimer > 0) {
            int cY = 20 + (tCount * 65);
            DrawRectangle(hudX, cY, 240, 60, Fade(BLACK, 0.6f));
            DrawRectangle(hudX + 10, cY + 35, (int)(220 * (e.hp / e.maxHp)), 12, RED);
            DrawText(TextFormat("[%s ENEMY]", eNames[e.type]), hudX + 10, cY + 10, 16, GOLD);
            tCount++; if (tCount >= 5) break;
        }
        for (auto& dt : dmgTexts) { Vector2 sPos = GetWorldToScreen(dt.pos, camera); DrawText(TextFormat("%d", dt.amount), (int)sPos.x - 10, (int)sPos.y, 22, YELLOW); }

        // 左下プレイヤーHP
        DrawRectangle(20, screenHeight - 50, 250, 30, Fade(BLACK, 0.5f));
        DrawRectangle(25, screenHeight - 45, (int)(240 * (fmax(0, player.hp) / player.maxHp)), 20, GREEN);
        DrawText(TextFormat("PLAYER HP: %.0f/100", player.hp), 35, screenHeight - 42, 18, WHITE);

        // --- 右下デバッグ・操作情報 ---
        int boxW = 220, boxH = 100;
        int startX = screenWidth - boxW - 10;
        int startY = screenHeight - boxH - 10;
        DrawRectangle(startX, startY, boxW, boxH, Fade(BLACK, 0.6f));
        DrawRectangleLines(startX, startY, boxW, boxH, DARKGRAY);
        DrawText(TextFormat("[F1] DEBUG: %s", debugMode ? "ON" : "OFF"), startX + 10, startY + 15, 18, debugMode ? RED : GREEN);
        DrawText("[ E ] SPAWN ENEMY", startX + 10, startY + 40, 18, RAYWHITE);
        DrawText("[ R ] RESET MAP", startX + 10, startY + 65, 18, RAYWHITE);

        EndDrawing();
    }
    CloseWindow(); return 0;
}