#include "Player.h"
#include "Dungeon.h"
#include "raymath.h"

Player::Player(Vector3 startPos) {
    position = startPos;
    speed = 0.15f;
    radius = 0.45f;
}

void Player::Update(Camera3D& camera, Dungeon& dungeon) {
    Vector3 forward = Vector3Subtract(camera.target, camera.position);
    forward.y = 0; forward = Vector3Normalize(forward);
    Vector3 right = { -forward.z, 0.0f, forward.x };

    Vector3 moveDir = { 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W)) moveDir = Vector3Add(moveDir, forward);
    if (IsKeyDown(KEY_S)) moveDir = Vector3Subtract(moveDir, forward);
    if (IsKeyDown(KEY_A)) moveDir = Vector3Subtract(moveDir, right);
    if (IsKeyDown(KEY_D)) moveDir = Vector3Add(moveDir, right);

    if (Vector3Length(moveDir) > 0) {
        moveDir = Vector3Normalize(moveDir);
        Vector3 velocity = Vector3Scale(moveDir, speed);
        Vector3 nextPosX = { position.x + velocity.x, position.y, position.z };
        if (!dungeon.CheckCollisionRadius(nextPosX, radius)) position.x = nextPosX.x;
        Vector3 nextPosZ = { position.x, position.y, position.z + velocity.z };
        if (!dungeon.CheckCollisionRadius(nextPosZ, radius)) position.z = nextPosZ.z;
    }
}

void Player::Draw() {
    DrawCube(position, 1.0f, 1.0f, 1.0f, RED);
    DrawCubeWires(position, 1.0f, 1.0f, 1.0f, MAROON);
}