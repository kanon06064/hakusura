#pragma once
#include "raylib.h"
#include <vector>

enum TileType { TILE_FLOOR = 0, TILE_WALL = 1 };

struct Room {
    int x, y, width, height;
    Vector2 GetCenter() { return { (float)x + (float)width / 2.0f, (float)y + (float)height / 2.0f }; }
    bool Contains(int gridX, int gridY) {
        return (gridX >= x && gridX < x + width && gridY >= y && gridY < y + height);
    }
};

class Dungeon {
public:
    static const int MAP_WIDTH = 40;
    static const int MAP_HEIGHT = 40;
    const float TILE_SIZE = 2.0f;

    Dungeon();
    void Generate();
    void Draw();
    bool IsWall(float x, float z);
    bool CheckCollisionRadius(Vector3 pos, float radius);
    bool HasLineOfSight(Vector3 start, Vector3 end);
    bool IsDiscovered(float x, float z);
    Vector3 GetStartPosition();
    Vector3 GetRandomFloorPos();
    void UpdateVisibility(Vector3 playerPos);

private:
    int map[MAP_WIDTH][MAP_HEIGHT];
    bool discovered[MAP_WIDTH][MAP_HEIGHT];
    std::vector<Room> rooms;
    void DigCorridor(int x1, int y1, int x2, int y2);
};