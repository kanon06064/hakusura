#pragma once
#include "Definitions.h"
#include "raymath.h"
#include <vector>

// ダンジョン内の「部屋」を表す構造体
struct Room {
    int x, y, width, height;
    RoomType type; // 部屋の種類（通常、宝物庫、ボス部屋など）
    bool Contains(int gx, int gy); // 指定座標が部屋の中か判定
    Vector3 GetCenter() const;     // 部屋の中心の3Dワールド座標を取得
};

class Dungeon {
public:
    // --- 重要な施設やオブジェクトの座標 ---
    Vector3 stairsDownPos, stairsUpPos, storageBoxPos;
    Vector3 portalPos;          // 拠点への帰還ポータル
    Vector3 healStationPos;     // 回復の泉
    Vector3 reforgeStationPos;  // リフォージ施設
    Vector3 craftStationPos;    // クラフト施設
    Vector3 bossSpawnPos;       // ボス出現位置
    Vector3 questBoardPos;      // クエストボード

    Vector3 dungeonEntrances[3]; // ホームにある3つのダンジョンへの入り口
    int currentDungeonId;        // 現在潜っているダンジョンのID

    std::vector<Vector3> treasureSpots; // アイテムが落ちている宝の座標リスト

    bool isHome; // 現在のマップが拠点(ホーム)かどうか
    int currentWidth;
    int currentHeight;

    Dungeon();

    // マップを生成するメイン関数
    void Generate(bool homeMode, int floor, int dungeonId = 0, int unlockedDungeonId = 0);
    void Draw(); // 3D空間にマップを描画する

    // 当たり判定・視界判定用の関数群
    bool IsWall(float x, float z);
    bool CheckCollisionRadius(Vector3 pos, float radius); // 円形コライダーで壁との衝突を判定
    bool HasLineOfSight(Vector3 start, Vector3 end);      // 敵からプレイヤーが見えるか(射線判定)
    bool IsDiscovered(float x, float z);                  // プレイヤーが探索済みのマスか
    void UpdateVisibility(Vector3 playerPos);             // 視界(探索範囲)を広げる

    Vector3 GetStartPosition();  // 階層開始時のプレイヤー位置を取得
    Vector3 GetRandomFloorPos(); // 敵やアイテムをスポーンさせるためのランダムな床座標を取得

private:
    std::vector<std::vector<int>> map;        // マップのグリッドデータ (0:床, 1:壁, 2:何もない空間)
    std::vector<std::vector<bool>> discovered; // 探索済みフラグ(ミニマップ用)
    std::vector<Room> rooms;

    void GenerateRestFloor();       // 5階ごとの休憩(安全)フロアを生成
    void GenerateBossFloor();       // 10階ごとのボスフロアを生成
    void GenerateNormalFloor(int floor); // 通常のランダムダンジョン生成
    void OptimizeMap();             // 繋がっていない不要な壁を削除する最適化
    void DigCorridor(int x1, int y1, int x2, int y2); // 部屋と部屋を通路で繋ぐ
    void SnapToTile(Vector3& pos);  // 座標をグリッドの中央に補正する
};