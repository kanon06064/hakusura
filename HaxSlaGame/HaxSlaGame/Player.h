#pragma once
#include "raylib.h"

class Dungeon; // ‘O•ûéŒ¾

class Player {
public:
    Vector3 position;
    float speed;
    float radius; // “–‚½‚è”»’è‚Ì‘å‚«‚³

    Player(Vector3 startPos);
    void Update(Camera3D& camera, Dungeon& dungeon);
    void Draw();
};