#include "Player.h"
#include "Dungeon.h"
#include "raymath.h"

Player::Player(Vector3 startPos) {
    position = startPos;
    speed = 0.15f;
    radius = 0.4f;
}

void Player::Update(Camera3D& camera, Dungeon& dungeon) {
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    forward.y = 0;
    forward = Vector3Normalize(forward);
    Vector3 right = { -forward.z, 0.0f, forward.x };

    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) moveDir = Vector3Add(moveDir, forward);
    if (IsKeyDown(KEY_S)) moveDir = Vector3Subtract(moveDir, forward);
    if (IsKeyDown(KEY_A)) moveDir = Vector3Subtract(moveDir, right);
    if (IsKeyDown(KEY_D)) moveDir = Vector3Add(moveDir, right);

    if (Vector3Length(moveDir) > 0) {
        moveDir = Vector3Normalize(moveDir);
        Vector3 next = Vector3Add(position, Vector3Scale(moveDir, speed));

        // 半径を考慮した4点チェック
        if (!dungeon.IsWall(next.x + radius, position.z) && !dungeon.IsWall(next.x - radius, position.z) &&
            !dungeon.IsWall(next.x, position.z + radius) && !dungeon.IsWall(next.x, position.z - radius)) {
            position.x = next.x;
        }
        if (!dungeon.IsWall(position.x + radius, next.z) && !dungeon.IsWall(position.x - radius, next.z) &&
            !dungeon.IsWall(position.x, next.z + radius) && !dungeon.IsWall(position.x, next.z - radius)) {
            position.z = next.z;
        }
    }
}

void Player::Draw() {
    DrawCube(position, 1.0f, 1.0f, 1.0f, RED);
    DrawCubeWires(position, 1.0f, 1.0f, 1.0f, MAROON);
}