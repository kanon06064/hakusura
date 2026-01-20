#pragma once
#include "raylib.h"
#include <vector>

// マップのタイルの種類
enum TileType {
    TILE_FLOOR = 0,
    TILE_WALL = 1
};

// 部屋の情報を保持する構造体
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
    void Generate();                 // ダンジョン生成（到達不能エリア削除含む）
    void Draw();                     // 描画（視界内のみ）
    bool IsWall(float x, float z);   // 当たり判定
    Vector3 GetStartPosition();      // プレイヤーの初期位置取得
    void UpdateVisibility(Vector3 playerPos); // 視界の更新

private:
    int map[MAP_WIDTH][MAP_HEIGHT];
    bool discovered[MAP_WIDTH][MAP_HEIGHT]; // 探索済みフラグ
    std::vector<Room> rooms;

    void DigCorridor(int x1, int y1, int x2, int y2);
};