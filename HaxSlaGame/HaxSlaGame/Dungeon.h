#pragma once
#include "Definitions.h"
#include "raymath.h"
#include <vector>

struct Room {
    int x, y, width, height;
    RoomType type;
    bool Contains(int gx, int gy);
    Vector3 GetCenter() const;
};

class Dungeon {
public:
    Vector3 stairsDownPos, stairsUpPos, storageBoxPos;
    Vector3 portalPos;
    Vector3 healStationPos;
    Vector3 reforgeStationPos;
    Vector3 craftStationPos;
    Vector3 bossSpawnPos;
    Vector3 questBoardPos;

    Vector3 dungeonEntrances[3];
    int currentDungeonId;

    std::vector<Vector3> treasureSpots;

    //  各フロアに配置される素材採取スポット一覧
    std::vector<GatherSpot> gatherSpots;

    bool isHome;
    int currentWidth;
    int currentHeight;

    Dungeon();

    void Generate(bool homeMode, int floor, int dungeonId = 0, int unlockedDungeonId = 0);
    void Draw(bool showExit = true);

    bool IsWall(float x, float z);
    bool CheckCollisionRadius(Vector3 pos, float radius);
    bool HasLineOfSight(Vector3 start, Vector3 end);
    bool IsDiscovered(float x, float z);
    void UpdateVisibility(Vector3 playerPos);
    void CastVisibilityRay(int x0, int y0, int x1, int y1);

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
    void SnapToTile(Vector3& pos);
};