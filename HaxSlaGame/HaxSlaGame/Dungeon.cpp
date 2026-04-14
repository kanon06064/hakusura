#include "Dungeon.h"
#include "DataManager.h" 
#include "raymath.h"
#include <math.h>
#include <algorithm>
#include <vector>

bool Room::Contains(int gx, int gy) { return (gx >= x && gx < x + width && gy >= y && gy < y + height); }
Vector3 Room::GetCenter() const { return { (float)x * TILE_SIZE + (float)width * TILE_SIZE / 2.0f, 0.5f, (float)y * TILE_SIZE + (float)height * TILE_SIZE / 2.0f }; }

Dungeon::Dungeon() {
    map.resize(MAX_MAP_WIDTH, std::vector<int>(MAX_MAP_HEIGHT, 2));
    discovered.resize(MAX_MAP_WIDTH, std::vector<bool>(MAX_MAP_HEIGHT, false));
    Generate(true, 0);
}

void Dungeon::Generate(bool homeMode, int floor) {
    isHome = homeMode; rooms.clear(); treasureSpots.clear();
    stairsDownPos = { -999,-999,-999 }; stairsUpPos = { -999,-999,-999 };
    storageBoxPos = { -999,-999,-999 }; portalPos = { -999,-999,-999 };
    healStationPos = { -999,-999,-999 }; reforgeStationPos = { -999,-999,-999 };
    craftStationPos = { -999,-999,-999 }; bossSpawnPos = { -999,-999,-999 };
    questBoardPos = { -999,-999,-999 };

    if (homeMode) { currentWidth = 60; currentHeight = 60; }
    else {
        currentWidth = 40 + floor; currentHeight = 40 + floor;
        if (currentWidth > MAX_MAP_WIDTH) currentWidth = MAX_MAP_WIDTH;
        if (currentHeight > MAX_MAP_HEIGHT) currentHeight = MAX_MAP_HEIGHT;
    }

    for (int x = 0; x < MAX_MAP_WIDTH; x++) for (int y = 0; y < MAX_MAP_HEIGHT; y++) { map[x][y] = 2; discovered[x][y] = homeMode; }

    if (homeMode) {
        int cx = 30, cy = 30;

        // 1. 中央部屋 (ポータル, ストレージ, 階段)
        Room centerRoom = { cx - 5, cy - 5, 10, 10, RT_NORMAL };
        rooms.push_back(centerRoom);
        for (int ry = centerRoom.y; ry < centerRoom.y + centerRoom.height; ry++)
            for (int rx = centerRoom.x; rx < centerRoom.x + centerRoom.width; rx++) map[rx][ry] = 0;

        // 2. 上の部屋 (クラフト)
        Room topRoom = { cx - 4, cy - 18, 8, 8, RT_NORMAL };
        rooms.push_back(topRoom);
        for (int ry = topRoom.y; ry < topRoom.y + topRoom.height; ry++)
            for (int rx = topRoom.x; rx < topRoom.x + topRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx, cy - 14);

        // 3. 下の部屋 (回復)
        Room bottomRoom = { cx - 4, cy + 10, 8, 8, RT_NORMAL };
        rooms.push_back(bottomRoom);
        for (int ry = bottomRoom.y; ry < bottomRoom.y + bottomRoom.height; ry++)
            for (int rx = bottomRoom.x; rx < bottomRoom.x + bottomRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx, cy + 14);

        // 4. 左の部屋 (リフォージ)
        Room leftRoom = { cx - 18, cy - 4, 8, 8, RT_NORMAL };
        rooms.push_back(leftRoom);
        for (int ry = leftRoom.y; ry < leftRoom.y + leftRoom.height; ry++)
            for (int rx = leftRoom.x; rx < leftRoom.x + leftRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx - 14, cy);

        // 5. 右の部屋 (クエストボード)
        Room rightRoom = { cx + 10, cy - 4, 8, 8, RT_NORMAL };
        rooms.push_back(rightRoom);
        for (int ry = rightRoom.y; ry < rightRoom.y + rightRoom.height; ry++)
            for (int rx = rightRoom.x; rx < rightRoom.x + rightRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx + 14, cy);

        // 施設の配置
        portalPos = centerRoom.GetCenter();
        portalPos.z -= 4.0f; // 奥

        storageBoxPos = centerRoom.GetCenter();
        storageBoxPos.x -= 4.0f; // 左

        // ★追加：ダンジョンの第1層へ向かう下り階段を配置
        stairsDownPos = centerRoom.GetCenter();
        stairsDownPos.x += 4.0f; // 右
        stairsDownPos.y = 0.1f;

        craftStationPos = topRoom.GetCenter();
        healStationPos = bottomRoom.GetCenter();
        reforgeStationPos = leftRoom.GetCenter();
        questBoardPos = rightRoom.GetCenter();

        OptimizeMap();
    }
    else {
        if (floor > 0 && floor % 10 == 0) GenerateBossFloor();
        else if (floor > 0 && floor % 10 == 5) GenerateRestFloor();
        else GenerateNormalFloor(floor);
        OptimizeMap();
    }
}

void Dungeon::OptimizeMap() {
    for (int y = 0; y < currentHeight; y++) for (int x = 0; x < currentWidth; x++) {
        if (map[x][y] == 2) {
            bool nF = false; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx; int ny = y + dy; if (nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight && map[nx][ny] == 0) nF = true;
            }
            if (nF) map[x][y] = 1;
        }
    }
}
void Dungeon::GenerateRestFloor() {
    int cx = currentWidth / 2; int cy = currentHeight / 2;
    Room r1 = { cx - 5,cy + 5,10,10,RT_NORMAL }; Room r2 = { cx - 5,cy - 5,10,8,RT_HEAL }; Room r3 = { cx - 5,cy - 15,10,8,RT_NORMAL };
    rooms.push_back(r1); rooms.push_back(r2); rooms.push_back(r3);
    for (auto& r : rooms) for (int ry = r.y; ry < r.y + r.height; ry++) for (int rx = r.x; rx < r.x + r.width; rx++) map[rx][ry] = 0;
    DigCorridor(r1.GetCenter().x / TILE_SIZE, r1.GetCenter().z / TILE_SIZE, r2.GetCenter().x / TILE_SIZE, r2.GetCenter().z / TILE_SIZE);
    DigCorridor(r2.GetCenter().x / TILE_SIZE, r2.GetCenter().z / TILE_SIZE, r3.GetCenter().x / TILE_SIZE, r3.GetCenter().z / TILE_SIZE);
    stairsUpPos = r1.GetCenter(); stairsUpPos.y = 0.1f; portalPos = Vector3Add(stairsUpPos, { 3,0,0 });
    healStationPos = r2.GetCenter(); stairsDownPos = r3.GetCenter(); stairsDownPos.y = 0.1f;
}
void Dungeon::GenerateBossFloor() {
    int cx = currentWidth / 2; int cy = currentHeight / 2;
    Room r1 = { cx - 6,cy + 10,12,8,RT_HEAL }; Room r2 = { cx - 15,cy - 15,30,20,RT_BOSS };
    rooms.push_back(r1); rooms.push_back(r2);
    for (auto& r : rooms) for (int ry = r.y; ry < r.y + r.height; ry++) for (int rx = r.x; rx < r.x + r.width; rx++) map[rx][ry] = 0;
    DigCorridor(r1.GetCenter().x / TILE_SIZE, r1.GetCenter().z / TILE_SIZE, r2.GetCenter().x / TILE_SIZE, r2.GetCenter().z / TILE_SIZE);
    stairsUpPos = r1.GetCenter(); stairsUpPos.y = 0.1f; healStationPos = Vector3Add(stairsUpPos, { -3,0,0 }); portalPos = Vector3Add(stairsUpPos, { 3,0,0 });
    bossSpawnPos = r2.GetCenter(); stairsDownPos = r2.GetCenter(); stairsDownPos.y = 0.1f;
}
void Dungeon::GenerateNormalFloor(int floor) {
    std::vector<RoomType> typeQueue = { RT_SMALL, RT_NORMAL, RT_LARGE, RT_TREASURE };
    int ex = 0; if (floor % 10 >= 1 && floor % 10 <= 4) ex = GetRandomValue(2, 3); else ex = GetRandomValue(3, 5);
    if (floor > 10) ex += (floor - 10) / 5;
    int tR = 4 + ex; if (tR > 20) tR = 20;
    while ((int)typeQueue.size() < tR) { int r = GetRandomValue(0, 100); if (r < 30)typeQueue.push_back(RT_SMALL); else if (r < 60)typeQueue.push_back(RT_NORMAL); else if (r < 80)typeQueue.push_back(RT_LARGE); else typeQueue.push_back(RT_TREASURE); }
    for (auto t : typeQueue) {
        int w, h; switch (t) { case RT_SMALL:w = GetRandomValue(4, 6); h = w; break; case RT_NORMAL:w = GetRandomValue(7, 9); h = w; break; case RT_LARGE:w = GetRandomValue(12, 16); h = w; break; case RT_TREASURE:w = 4; h = 4; break; default:w = 5; h = 5; break; }
                                            for (int k = 0; k < 50; k++) {
                                                int x = GetRandomValue(1, currentWidth - w - 1); int y = GetRandomValue(1, currentHeight - h - 1);
                                                bool ov = false; for (auto& r : rooms) if (x<r.x + r.width + 2 && x + w + 2>r.x && y<r.y + r.height + 2 && y + h + 2>r.y) { ov = true; break; }
                                                if (!ov) {
                                                    rooms.push_back({ x,y,w,h,t }); for (int ry = y; ry < y + h; ry++)for (int rx = x; rx < x + w; rx++)map[rx][ry] = 0;
                                                    if (rooms.size() > 1) { Room& p = rooms[rooms.size() - 2]; DigCorridor(p.x + p.width / 2, p.y + p.height / 2, x + w / 2, y + h / 2); }
                                                    if (t == RT_TREASURE) treasureSpots.push_back({ (float)(x * TILE_SIZE + w * TILE_SIZE / 2),0.5f,(float)(y * TILE_SIZE + h * TILE_SIZE / 2) });
                                                    break;
                                                }
                                            }
    }
    std::vector<int> v; for (int i = 0; i < (int)rooms.size(); i++) if (rooms[i].type != RT_TREASURE) v.push_back(i);
    if (v.size() < 2) { v.clear(); for (int i = 0; i < (int)rooms.size(); i++)v.push_back(i); }
    if (v.size() >= 1) { stairsUpPos = rooms[v[0]].GetCenter(); stairsUpPos.y = 0.1f; stairsDownPos = rooms[v.size() > 1 ? v.back() : v[0]].GetCenter(); stairsDownPos.y = 0.1f; }
}
void Dungeon::DigCorridor(int x1, int y1, int x2, int y2) {
    x1 = (int)fmaxf(0, fminf((float)x1, (float)currentWidth - 1)); x2 = (int)fmaxf(0, fminf((float)x2, (float)currentWidth - 1));
    y1 = (int)fmaxf(0, fminf((float)y1, (float)currentHeight - 1)); y2 = (int)fmaxf(0, fminf((float)y2, (float)currentHeight - 1));
    for (int x = (int)fminf(x1, x2); x <= (int)fmaxf(x1, x2); x++) map[x][y1] = 0;
    for (int y = (int)fminf(y1, y2); y <= (int)fmaxf(y1, y2); y++) map[x2][y] = 0;
}
void Dungeon::UpdateVisibility(Vector3 p) {
    if (isHome)return; int gx = (int)floorf((p.x + TILE_SIZE / 2) / TILE_SIZE); int gz = (int)floorf((p.z + TILE_SIZE / 2) / TILE_SIZE);
    for (int y = gz - 5; y <= gz + 5; y++)for (int x = gx - 5; x <= gx + 5; x++) if (x >= 0 && x < currentWidth && y >= 0 && y < currentHeight) discovered[x][y] = true;
}

void Dungeon::Draw() {
    for (int y = 0; y < currentHeight; y++) for (int x = 0; x < currentWidth; x++) {
        if (!discovered[x][y]) continue; if (map[x][y] == 2) continue;
        Vector3 pos = { (float)x * TILE_SIZE, 0.0f, (float)y * TILE_SIZE };
        if (map[x][y] == 1) DrawCube(pos, TILE_SIZE, 2.0f, TILE_SIZE, GRAY); else DrawPlane(pos, { TILE_SIZE, TILE_SIZE }, isHome ? DARKBLUE : DARKGREEN);
    }

    auto drawEnv = [](const std::string& key, Vector3 pos) -> bool {
        if (DataManager::loadedModels.count(key) > 0) {
            DrawModel(DataManager::loadedModels[key].model, pos, 1.0f, WHITE);
            return true;
        }
        return false;
        };

    if (stairsDownPos.x != -999 && IsDiscovered(stairsDownPos.x, stairsDownPos.z)) {
        if (!drawEnv("Obj_StairsDown", stairsDownPos)) DrawCube(stairsDownPos, 1.2f, 0.1f, 1.2f, GOLD);
    }

    if (stairsUpPos.x != -999 && IsDiscovered(stairsUpPos.x, stairsUpPos.z)) {
        if (!drawEnv("Obj_StairsUp", stairsUpPos)) DrawCube(stairsUpPos, 1.2f, 0.1f, 1.2f, SKYBLUE);
    }

    if (isHome && storageBoxPos.x != -999) {
        if (!drawEnv("Obj_Storage", storageBoxPos)) {
            DrawCube(storageBoxPos, 1.2f, 1.2f, 1.2f, BROWN);
            DrawCubeWires(storageBoxPos, 1.2f, 1.2f, 1.2f, BLACK);
        }
    }

    if (isHome && reforgeStationPos.x != -999) {
        if (!drawEnv("Obj_Reforge", reforgeStationPos)) {
            DrawCube(reforgeStationPos, 1.2f, 1.2f, 1.2f, PURPLE);
            DrawCubeWires(reforgeStationPos, 1.2f, 1.2f, 1.2f, VIOLET);
        }
    }

    if (isHome && craftStationPos.x != -999) {
        if (!drawEnv("Obj_Craft", craftStationPos)) {
            DrawCube(craftStationPos, 1.2f, 1.2f, 1.2f, ORANGE);
            DrawCubeWires(craftStationPos, 1.2f, 1.2f, 1.2f, YELLOW);
        }
    }

    if (portalPos.x != -999 && IsDiscovered(portalPos.x, portalPos.z)) {
        if (!drawEnv("Obj_Portal", portalPos)) {
            DrawCylinder(portalPos, 1.0f, 1.0f, 2.5f, 8, Fade(PURPLE, 0.8f));
            DrawCylinderWires(portalPos, 1.0f, 1.0f, 2.5f, 8, VIOLET);
        }
    }

    if (healStationPos.x != -999 && IsDiscovered(healStationPos.x, healStationPos.z)) {
        if (!drawEnv("Obj_Heal", healStationPos)) {
            DrawCube(healStationPos, 1.5f, 0.5f, 1.5f, PINK);
            Vector3 crossV = Vector3Add(healStationPos, { 0, 0.5f, 0 });
            DrawCube(crossV, 0.3f, 1.0f, 0.3f, RED);
            DrawCube(crossV, 1.0f, 0.3f, 0.3f, RED);
        }
    }

    if (isHome && questBoardPos.x != -999) {
        if (!drawEnv("Obj_QuestBoard", questBoardPos)) {
            DrawCube(questBoardPos, 2.0f, 2.0f, 0.2f, BEIGE);
            DrawCubeWires(questBoardPos, 2.0f, 2.0f, 0.2f, DARKBROWN);
        }
    }
}

bool Dungeon::IsWall(float x, float z) { int gx = (int)floorf((x + TILE_SIZE / 2) / TILE_SIZE); int gz = (int)floorf((z + TILE_SIZE / 2) / TILE_SIZE); if (gx < 0 || gx >= currentWidth || gz < 0 || gz >= currentHeight)return true; return map[gx][gz] != 0; }
bool Dungeon::CheckCollisionRadius(Vector3 p, float r) { return IsWall(p.x - r, p.z) || IsWall(p.x + r, p.z) || IsWall(p.x, p.z - r) || IsWall(p.x, p.z + r); }
bool Dungeon::HasLineOfSight(Vector3 s, Vector3 e) { float d = Vector3Distance(s, e); Vector3 dir = Vector3Normalize(Vector3Subtract(e, s)); for (float t = 0.5f; t < d; t += 0.5f) { Vector3 p = Vector3Add(s, Vector3Scale(dir, t)); if (IsWall(p.x, p.z))return false; }return true; }
bool Dungeon::IsDiscovered(float x, float z) { int gx = (int)floorf((x + TILE_SIZE / 2) / TILE_SIZE); int gz = (int)floorf((z + TILE_SIZE / 2) / TILE_SIZE); return (gx >= 0 && gx < currentWidth && gz >= 0 && gz < currentHeight) ? discovered[gx][gz] : false; }
Vector3 Dungeon::GetStartPosition() { if (stairsUpPos.x != -999)return stairsUpPos; return rooms.empty() ? Vector3{ 0,0,0 } : rooms[0].GetCenter(); }

Vector3 Dungeon::GetRandomFloorPos() {
    for (int i = 0; i < 50; i++) {
        int r = GetRandomValue(0, (int)rooms.size() - 1);
        if (rooms[r].type == RT_TREASURE || rooms[r].type == RT_HEAL || rooms[r].type == RT_BOSS)continue;

        int tx = GetRandomValue(rooms[r].x + 1, rooms[r].x + rooms[r].width - 2);
        int ty = GetRandomValue(rooms[r].y + 1, rooms[r].y + rooms[r].height - 2);
        Vector3 c = { (float)tx * TILE_SIZE + TILE_SIZE / 2.0f, 0.5f, (float)ty * TILE_SIZE + TILE_SIZE / 2.0f };

        if (stairsUpPos.x != -999 && Vector3Distance(c, stairsUpPos) < 8.0f)continue;
        if (stairsDownPos.x != -999 && Vector3Distance(c, stairsDownPos) < 5.0f)continue;

        if (!IsWall(c.x, c.z)) return c;
    }
    return{ 0,0,0 };
}