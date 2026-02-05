#include "Dungeon.h"
#include "raymath.h"
#include <math.h>
#include <algorithm>
#include <vector>

bool Room::Contains(int gx, int gy) { return (gx >= x && gx < x + width && gy >= y && gy < y + height); }

Vector3 Room::GetCenter() const {
    return { (float)x * TILE_SIZE + (float)width * TILE_SIZE / 2.0f, 0.5f, (float)y * TILE_SIZE + (float)height * TILE_SIZE / 2.0f };
}

Dungeon::Dungeon() { Generate(true, 0); }

void Dungeon::Generate(bool homeMode, int floor) {
    isHome = homeMode;
    rooms.clear();
    treasureSpots.clear();

    stairsDownPos = { -999,-999,-999 };
    stairsUpPos = { -999,-999,-999 };
    storageBoxPos = { -999,-999,-999 };
    portalPos = { -999,-999,-999 };
    healStationPos = { -999,-999,-999 };
    reforgeStationPos = { -999,-999,-999 };

    if (homeMode) {
        currentWidth = 40;
        currentHeight = 40;
    }
    else {
        currentWidth = 40 + (floor * 2);
        currentHeight = 40 + (floor * 2);
        if (currentWidth > MAX_MAP_WIDTH) currentWidth = MAX_MAP_WIDTH;
        if (currentHeight > MAX_MAP_HEIGHT) currentHeight = MAX_MAP_HEIGHT;
    }

    for (int y = 0; y < MAX_MAP_HEIGHT; y++)
        for (int x = 0; x < MAX_MAP_WIDTH; x++) {
            map[x][y] = 1;
            discovered[x][y] = homeMode;
        }

    if (homeMode) {
        Room c = { currentWidth / 2 - 5, currentHeight / 2 - 5, 10, 10, RT_NORMAL };
        rooms.push_back(c);
        for (int ry = c.y; ry < c.y + c.height; ry++) for (int rx = c.x; rx < c.x + c.width; rx++) map[rx][ry] = 0;

        stairsDownPos = { (float)(c.x + 8) * TILE_SIZE, 0.1f, (float)(c.y + 8) * TILE_SIZE };
        storageBoxPos = { (float)(c.x + 2) * TILE_SIZE, 0.5f, (float)(c.y + 2) * TILE_SIZE };
        reforgeStationPos = { (float)(c.x + 5) * TILE_SIZE, 0.5f, (float)(c.y + 2) * TILE_SIZE };
    }
    else {
        std::vector<RoomType> typeQueue;
        typeQueue.push_back(RT_SMALL);
        typeQueue.push_back(RT_NORMAL);
        typeQueue.push_back(RT_LARGE);
        typeQueue.push_back(RT_TREASURE);
        typeQueue.push_back(RT_HEAL);

        int baseRooms = 7 + (floor / 5);
        int totalRooms = GetRandomValue(baseRooms, baseRooms + 2);
        if (totalRooms < 5) totalRooms = 5;

        while ((int)typeQueue.size() < totalRooms) {
            int rnd = GetRandomValue(0, 100);
            if (rnd < 20) typeQueue.push_back(RT_SMALL);
            else if (rnd < 50) typeQueue.push_back(RT_NORMAL);
            else if (rnd < 70) typeQueue.push_back(RT_LARGE);
            else if (rnd < 90) typeQueue.push_back(RT_TREASURE);
            else typeQueue.push_back(RT_HEAL);
        }

        bool spawnPortal = (floor > 0 && floor % 5 == 0);

        for (RoomType rType : typeQueue) {
            int w, h;
            switch (rType) {
            case RT_SMALL:    w = GetRandomValue(4, 6); h = GetRandomValue(4, 6); break;
            case RT_NORMAL:   w = GetRandomValue(7, 9); h = GetRandomValue(7, 9); break;
            case RT_LARGE:    w = GetRandomValue(12, 16); h = GetRandomValue(12, 16); break;
            case RT_TREASURE: w = 4; h = 4; break;
            case RT_HEAL:     w = 6; h = 6; break;
            default:          w = 5; h = 5; break;
            }

            for (int t = 0; t < 50; t++) {
                int x = GetRandomValue(1, currentWidth - w - 1);
                int y = GetRandomValue(1, currentHeight - h - 1);
                Room nr = { x, y, w, h, rType };
                bool overlap = false;
                for (auto& r : rooms) {
                    if (x < r.x + r.width + 2 && x + w + 2 > r.x &&
                        y < r.y + r.height + 2 && y + h + 2 > r.y) {
                        overlap = true; break;
                    }
                }

                if (!overlap) {
                    rooms.push_back(nr);
                    for (int ry = y; ry < y + h; ry++) for (int rx = x; rx < x + w; rx++) map[rx][ry] = 0;
                    if ((int)rooms.size() > 1) {
                        Room& prev = rooms[rooms.size() - 2];
                        DigCorridor(prev.x + prev.width / 2, prev.y + prev.height / 2, x + w / 2, y + h / 2);
                    }
                    if (rType == RT_TREASURE) {
                        treasureSpots.push_back(nr.GetCenter());
                    }
                    if (rType == RT_HEAL) {
                        if (healStationPos.x == -999) {
                            healStationPos = nr.GetCenter();
                            if (spawnPortal) {
                                portalPos = Vector3Add(healStationPos, { 2.0f, 0, 2.0f });
                            }
                        }
                    }
                    break;
                }
            }
        }

        std::vector<int> validIndices;
        for (int i = 0; i < (int)rooms.size(); i++) {
            if (rooms[i].type != RT_TREASURE && rooms[i].type != RT_HEAL) {
                validIndices.push_back(i);
            }
        }
        if (validIndices.size() < 2) {
            validIndices.clear();
            for (int i = 0; i < (int)rooms.size(); i++) validIndices.push_back(i);
        }
        if (validIndices.size() >= 1) {
            int idxUp = validIndices[0];
            stairsUpPos = rooms[idxUp].GetCenter();
            stairsUpPos.y = 0.1f;
            int idxDown = validIndices.size() > 1 ? validIndices[validIndices.size() - 1] : validIndices[0];
            stairsDownPos = rooms[idxDown].GetCenter();
            stairsDownPos.y = 0.1f;
        }
    }
}

void Dungeon::DigCorridor(int x1, int y1, int x2, int y2) {
    x1 = (int)fmaxf(0, fminf((float)x1, (float)currentWidth - 1));
    x2 = (int)fmaxf(0, fminf((float)x2, (float)currentWidth - 1));
    y1 = (int)fmaxf(0, fminf((float)y1, (float)currentHeight - 1));
    y2 = (int)fmaxf(0, fminf((float)y2, (float)currentHeight - 1));
    for (int x = (int)fminf((float)x1, (float)x2); x <= (int)fmaxf((float)x1, (float)x2); x++) map[x][y1] = 0;
    for (int y = (int)fminf((float)y1, (float)y2); y <= (int)fmaxf((float)y1, (float)y2); y++) map[x2][y] = 0;
}

void Dungeon::UpdateVisibility(Vector3 p) {
    if (isHome) return;
    int gx = (int)floorf((p.x + TILE_SIZE / 2.0f) / TILE_SIZE), gz = (int)floorf((p.z + TILE_SIZE / 2.0f) / TILE_SIZE);
    for (int y = gz - 5; y <= gz + 5; y++)
        for (int x = gx - 5; x <= gx + 5; x++)
            if (x >= 0 && x < currentWidth && y >= 0 && y < currentHeight) discovered[x][y] = true;
}

void Dungeon::Draw() {
    for (int y = 0; y < currentHeight; y++) for (int x = 0; x < currentWidth; x++) {
        if (!discovered[x][y]) continue;
        Vector3 pos = { x * TILE_SIZE, 0.0f, y * TILE_SIZE };
        if (map[x][y] == 1) DrawCube(pos, TILE_SIZE, 2.0f, TILE_SIZE, GRAY);
        else DrawPlane(pos, { TILE_SIZE, TILE_SIZE }, isHome ? DARKBLUE : DARKGREEN);
    }

    if (stairsDownPos.x != -999 && IsDiscovered(stairsDownPos.x, stairsDownPos.z))
        DrawCube(stairsDownPos, 1.2f, 0.1f, 1.2f, GOLD);
    if (stairsUpPos.x != -999 && IsDiscovered(stairsUpPos.x, stairsUpPos.z))
        DrawCube(stairsUpPos, 1.2f, 0.1f, 1.2f, SKYBLUE);

    if (isHome && storageBoxPos.x != -999) {
        DrawCube(storageBoxPos, 1.2f, 1.2f, 1.2f, BROWN);
        DrawCubeWires(storageBoxPos, 1.2f, 1.2f, 1.2f, BLACK);
    }
    if (isHome && reforgeStationPos.x != -999) {
        DrawCube(reforgeStationPos, 1.2f, 1.2f, 1.2f, PURPLE);
        DrawCubeWires(reforgeStationPos, 1.2f, 1.2f, 1.2f, VIOLET);
    }
    if (portalPos.x != -999 && IsDiscovered(portalPos.x, portalPos.z)) {
        DrawCylinder(portalPos, 1.0f, 1.0f, 2.5f, 8, Fade(PURPLE, 0.8f));
        DrawCylinderWires(portalPos, 1.0f, 1.0f, 2.5f, 8, VIOLET);
    }
    if (healStationPos.x != -999 && IsDiscovered(healStationPos.x, healStationPos.z)) {
        DrawCube(healStationPos, 1.5f, 0.5f, 1.5f, PINK);
        Vector3 crossV = Vector3Add(healStationPos, { 0, 0.5f, 0 });
        DrawCube(crossV, 0.3f, 1.0f, 0.3f, RED);
        DrawCube(crossV, 1.0f, 0.3f, 0.3f, RED);
    }
}

bool Dungeon::IsWall(float x, float z) {
    int gx = (int)floorf((x + TILE_SIZE / 2.0f) / TILE_SIZE), gz = (int)floorf((z + TILE_SIZE / 2.0f) / TILE_SIZE);
    if (gx < 0 || gx >= currentWidth || gz < 0 || gz >= currentHeight) return true;
    return map[gx][gz] == 1;
}

bool Dungeon::CheckCollisionRadius(Vector3 p, float r) { return IsWall(p.x - r, p.z) || IsWall(p.x + r, p.z) || IsWall(p.x, p.z - r) || IsWall(p.x, p.z + r); }

bool Dungeon::HasLineOfSight(Vector3 s, Vector3 e) {
    float dist = Vector3Distance(s, e); Vector3 dir = Vector3Normalize(Vector3Subtract(e, s));
    for (float d = 0.5f; d < dist; d += 0.5f) { Vector3 p = Vector3Add(s, Vector3Scale(dir, d)); if (IsWall(p.x, p.z)) return false; }
    return true;
}

bool Dungeon::IsDiscovered(float x, float z) {
    int gx = (int)floorf((x + TILE_SIZE / 2.0f) / TILE_SIZE), gz = (int)floorf((z + TILE_SIZE / 2.0f) / TILE_SIZE);
    return (gx >= 0 && gx < currentWidth && gz >= 0 && gz < currentHeight) ? discovered[gx][gz] : false;
}

Vector3 Dungeon::GetStartPosition() {
    if (stairsUpPos.x != -999) return stairsUpPos;
    return rooms.empty() ? Vector3{ 0,0,0 } : rooms[0].GetCenter();
}

Vector3 Dungeon::GetRandomFloorPos() {
    for (int attempt = 0; attempt < 50; attempt++) {
        int r = GetRandomValue(0, (int)rooms.size() - 1);
        if (rooms[r].type == RT_TREASURE || rooms[r].type == RT_HEAL) continue;

        Vector3 cand = {
            (float)GetRandomValue(rooms[r].x, rooms[r].x + rooms[r].width - 1) * TILE_SIZE,
            0.5f,
            (float)GetRandomValue(rooms[r].y, rooms[r].y + rooms[r].height - 1) * TILE_SIZE
        };

        if (stairsUpPos.x != -999 && Vector3Distance(cand, stairsUpPos) < 8.0f) continue;
        if (stairsDownPos.x != -999 && Vector3Distance(cand, stairsDownPos) < 5.0f) continue;

        return cand;
    }

    if (rooms.empty()) return { 0,0,0 };
    int r = 0;
    return { (float)rooms[r].x * TILE_SIZE, 0.5f, (float)rooms[r].y * TILE_SIZE };
}