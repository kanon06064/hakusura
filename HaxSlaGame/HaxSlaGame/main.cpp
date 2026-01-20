#include "raylib.h"
#include "raymath.h"
#include "Player.h"
#include "Dungeon.h"
#include <time.h>

int main() {
    InitWindow(1280, 720, "3D Hack & Slash - Complete System");
    SetTargetFPS(60);
    srand((unsigned int)time(NULL));

    Dungeon dungeon;
    Player player(dungeon.GetStartPosition());

    Camera3D camera = { 0 };
    camera.position = { 10.0f, 10.0f, 10.0f };
    camera.up = { 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    while (!WindowShouldClose()) {
        Vector3 offset = Vector3Subtract(camera.position, camera.target);
        player.Update(camera, dungeon);
        dungeon.UpdateVisibility(player.position);

        camera.target = player.position;
        camera.position = Vector3Add(camera.target, offset);

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || GetMouseWheelMove() != 0) {
            UpdateCamera(&camera, CAMERA_THIRD_PERSON);
        }
        if (IsKeyPressed(KEY_R)) {
            dungeon.Generate();
            player.position = dungeon.GetStartPosition();
        }

        BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(camera);
        dungeon.Draw();
        player.Draw();
        EndMode3D();
        DrawRectangle(10, 10, 250, 40, Fade(SKYBLUE, 0.5f));
        DrawText("R: Regenerate Map", 20, 20, 20, WHITE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}