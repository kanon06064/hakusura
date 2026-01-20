#pragma once
#include "raylib.h"

class Dungeon;

class Player {
public:
    Vector3 position;
    float speed;
    float radius;

    Player(Vector3 startPos);
    void Update(Camera3D& camera, Dungeon& dungeon);
    void Draw();
};