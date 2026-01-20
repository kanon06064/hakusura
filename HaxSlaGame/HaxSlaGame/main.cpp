#include "raylib.h"
#include "raymath.h"
#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include <vector>
#include <time.h>

int main() {
    InitWindow(1280, 720, "3D Hack & Slash - Debug & Spawning");
    SetTargetFPS(60);
    srand((unsigned int)time(NULL));

    Dungeon dungeon;
    Player player(dungeon.GetStartPosition());

    // --- 敵の設定 ---
    std::vector<Enemy> enemies;
    const float safeDistance = 15.0f;
    bool debugMode = false;

    // 初期エネミー生成
    for (int i = 0; i < 6; i++) {
        Vector3 spawnPos;
        do { spawnPos = dungeon.GetRandomFloorPos(); } while (Vector3Distance(spawnPos, player.position) < safeDistance);
        enemies.push_back(Enemy(spawnPos));
    }

    Camera3D camera = { 0 };
    camera.position = { 10.0f, 10.0f, 10.0f };
    camera.target = player.position;
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        Vector3 offset = Vector3Subtract(camera.position, camera.target);

        // 更新
        player.Update(camera, dungeon);
        dungeon.UpdateVisibility(player.position);
        for (auto& enemy : enemies) enemy.Update(player, dungeon);

        // カメラ更新
        camera.target = player.position;
        camera.position = Vector3Add(camera.target, offset);
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || GetMouseWheelMove() != 0) UpdateCamera(&camera, CAMERA_THIRD_PERSON);

        // --- 入力判定 ---

        // F1: デバッグモード切り替え
        if (IsKeyPressed(KEY_F1)) debugMode = !debugMode;

        // E: エネミーを1体スポーン
        if (IsKeyPressed(KEY_E)) {
            Vector3 spawnPos;
            do { spawnPos = dungeon.GetRandomFloorPos(); } while (Vector3Distance(spawnPos, player.position) < safeDistance);
            enemies.push_back(Enemy(spawnPos));
        }

        // R: マップリセット
        if (IsKeyPressed(KEY_R)) {
            dungeon.Generate();
            player.position = dungeon.GetStartPosition();
            enemies.clear();
            for (int i = 0; i < 6; i++) {
                Vector3 spawnPos;
                do { spawnPos = dungeon.GetRandomFloorPos(); } while (Vector3Distance(spawnPos, player.position) < safeDistance);
                enemies.push_back(Enemy(spawnPos));
            }
        }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        dungeon.Draw();

        // 敵の描画
        for (auto& enemy : enemies) {
            // デバッグモードがON、または発見済みのエリアにいる場合のみ描画
            if (debugMode || dungeon.IsDiscovered(enemy.position.x, enemy.position.z)) {
                enemy.Draw();
            }
        }

        player.Draw();
        EndMode3D();

        // UI
        DrawRectangle(10, 10, 450, 100, Fade(SKYBLUE, 0.5f));
        DrawText(TextFormat("Debug Mode: %s (F1 to toggle)", debugMode ? "ON" : "OFF"), 20, 20, 20, debugMode ? RED : WHITE);
        DrawText("E: Spawn Enemy / R: Reset Map", 20, 45, 20, WHITE);
        DrawText(TextFormat("Enemies count: %d", (int)enemies.size()), 20, 70, 20, YELLOW);

        EndDrawing();
    }
    CloseWindow();
    return 0;
}