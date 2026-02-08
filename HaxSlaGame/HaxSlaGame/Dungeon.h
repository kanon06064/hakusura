#pragma once
#include "Definitions.h"
#include "raymath.h"
#include <vector>

struct Room { int x, y, width, height; RoomType type; bool Contains(int gx, int gy); Vector3 GetCenter() const; };

class Dungeon {
public:
    Vector3 stairsDownPos, stairsUpPos, storageBoxPos;
    Vector3 portalPos;
    Vector3 healStationPos;
    Vector3 reforgeStationPos;
    Vector3 craftStationPos; // クラフトステーション
    Vector3 bossSpawnPos;
    std::vector<Vector3> treasureSpots;

    bool isHome;
    int currentWidth;
    int currentHeight;

    Dungeon();
    void Generate(bool homeMode, int floor);
    void Draw();
    bool IsWall(float x, float z);
    bool CheckCollisionRadius(Vector3 pos, float radius);
    bool HasLineOfSight(Vector3 start, Vector3 end);
    bool IsDiscovered(float x, float z);
    void UpdateVisibility(Vector3 playerPos);
    Vector3 GetStartPosition();
    Vector3 GetRandomFloorPos();

private:
    std::vector<std::vector<int>> map;
    std::vector<std::vector<bool>> discovered;
    std::vector<Room> rooms;
    void GenerateRestFloor();
    void GenerateBossFloor();
    void GenerateNormalFloor(int floor);
    void OptimizeMap();
    void DigCorridor(int x1, int y1, int x2, int y2);
};