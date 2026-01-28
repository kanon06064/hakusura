#pragma once
#include "Definitions.h"

struct Room { int x, y, width, height; bool Contains(int gx, int gy); };

class Dungeon {
public:
    Vector3 stairsDownPos, stairsUpPos;
    bool isHome;
    Dungeon();
    void Generate(bool homeMode);
    void Draw();
    bool IsWall(float x, float z);
    bool CheckCollisionRadius(Vector3 pos, float radius);
    bool HasLineOfSight(Vector3 start, Vector3 end);
    bool IsDiscovered(float x, float z);
    void UpdateVisibility(Vector3 playerPos);
    Vector3 GetStartPosition();
    Vector3 GetRandomFloorPos();
private:
    int map[MAP_WIDTH][MAP_HEIGHT];
    bool discovered[MAP_WIDTH][MAP_HEIGHT];
    std::vector<Room> rooms;
    void DigCorridor(int x1, int y1, int x2, int y2);
};