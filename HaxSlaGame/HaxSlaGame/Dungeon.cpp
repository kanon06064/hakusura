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
    Generate(true, 0, 0, 0); // 初期化時はホームを生成
}

void Dungeon::SnapToTile(Vector3& pos) {
    if (pos.x != -999) {
        pos.x = roundf(pos.x / TILE_SIZE) * TILE_SIZE;
        pos.z = roundf(pos.z / TILE_SIZE) * TILE_SIZE;
    }
}

// ダンジョンまたはホームを生成するメイン関数
void Dungeon::Generate(bool homeMode, int floor, int dungeonId, int unlockedDungeonId) {
    currentDungeonId = dungeonId;
    isHome = homeMode; rooms.clear(); treasureSpots.clear();

    // 全ての施設・階段の座標を初期化(非表示を示す -999 に設定)
    stairsDownPos = { -999,-999,-999 }; stairsUpPos = { -999,-999,-999 };
    storageBoxPos = { -999,-999,-999 }; portalPos = { -999,-999,-999 };
    healStationPos = { -999,-999,-999 }; reforgeStationPos = { -999,-999,-999 };
    craftStationPos = { -999,-999,-999 }; bossSpawnPos = { -999,-999,-999 };
    questBoardPos = { -999,-999,-999 };

    for (int i = 0; i < 3; i++) dungeonEntrances[i] = { -999,-999,-999 };

    if (homeMode) { currentWidth = 60; currentHeight = 60; }
    else {
        // 階層が深くなるほどマップが広くなる
        currentWidth = 40 + floor; currentHeight = 40 + floor;
        if (currentWidth > MAX_MAP_WIDTH) currentWidth = MAX_MAP_WIDTH;
        if (currentHeight > MAX_MAP_HEIGHT) currentHeight = MAX_MAP_HEIGHT;
    }

    // マップをすべて「壁・未探索」で塗りつぶす
    for (int x = 0; x < MAX_MAP_WIDTH; x++) for (int y = 0; y < MAX_MAP_HEIGHT; y++) { map[x][y] = 2; discovered[x][y] = homeMode; }

    if (homeMode) {
        // --- ホーム(拠点)の固定マップ生成 ---
        int cx = 30, cy = 30;

        Room centerRoom = { cx - 5, cy - 5, 10, 10, RT_NORMAL };
        rooms.push_back(centerRoom);
        for (int ry = centerRoom.y; ry < centerRoom.y + centerRoom.height; ry++)
            for (int rx = centerRoom.x; rx < centerRoom.x + centerRoom.width; rx++) map[rx][ry] = 0;

        Room topRoom = { cx - 4, cy - 18, 8, 8, RT_NORMAL };
        rooms.push_back(topRoom);
        for (int ry = topRoom.y; ry < topRoom.y + topRoom.height; ry++)
            for (int rx = topRoom.x; rx < topRoom.x + topRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx, cy - 14);

        Room bottomRoom = { cx - 4, cy + 10, 8, 8, RT_NORMAL };
        rooms.push_back(bottomRoom);
        for (int ry = bottomRoom.y; ry < bottomRoom.y + bottomRoom.height; ry++)
            for (int rx = bottomRoom.x; rx < bottomRoom.x + bottomRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx, cy + 14);

        Room leftRoom = { cx - 18, cy - 4, 8, 8, RT_NORMAL };
        rooms.push_back(leftRoom);
        for (int ry = leftRoom.y; ry < leftRoom.y + leftRoom.height; ry++)
            for (int rx = leftRoom.x; rx < leftRoom.x + leftRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx - 14, cy);

        Room rightRoom = { cx + 10, cy - 4, 8, 8, RT_NORMAL };
        rooms.push_back(rightRoom);
        for (int ry = rightRoom.y; ry < rightRoom.y + rightRoom.height; ry++)
            for (int rx = rightRoom.x; rx < rightRoom.x + rightRoom.width; rx++) map[rx][ry] = 0;
        DigCorridor(cx, cy, cx + 14, cy);

        // 各施設を配置
        portalPos = centerRoom.GetCenter(); portalPos.z -= 4.0f;
        storageBoxPos = centerRoom.GetCenter(); storageBoxPos.x -= 4.0f;
        craftStationPos = topRoom.GetCenter();
        healStationPos = bottomRoom.GetCenter();
        reforgeStationPos = leftRoom.GetCenter();
        questBoardPos = rightRoom.GetCenter();

        // 解放済みのダンジョン入り口を配置
        if (unlockedDungeonId >= 0) { dungeonEntrances[0] = centerRoom.GetCenter(); dungeonEntrances[0].x += 4.0f; dungeonEntrances[0].z -= 4.0f; dungeonEntrances[0].y = 0.0f; }
        if (unlockedDungeonId >= 1) { dungeonEntrances[1] = centerRoom.GetCenter(); dungeonEntrances[1].x += 4.0f; dungeonEntrances[1].z += 0.0f; dungeonEntrances[1].y = 0.0f; }
        if (unlockedDungeonId >= 2) { dungeonEntrances[2] = centerRoom.GetCenter(); dungeonEntrances[2].x += 4.0f; dungeonEntrances[2].z += 4.0f; dungeonEntrances[2].y = 0.0f; }

        OptimizeMap();
    }
    else {
        // --- ダンジョンの自動生成分岐 ---
        if (floor > 0 && floor % 10 == 0) GenerateBossFloor();       // 10階ごと: ボス
        else if (floor > 0 && floor % 10 == 5) GenerateRestFloor();  // 5階ごと: 休憩
        else GenerateNormalFloor(floor);                             // その他: 通常階
        OptimizeMap();
    }

    // 施設や階段が壁やマス目にめり込まないよう、グリッド中央にスナップさせる
    SnapToTile(stairsDownPos); SnapToTile(stairsUpPos); SnapToTile(storageBoxPos);
    SnapToTile(portalPos); SnapToTile(healStationPos); SnapToTile(reforgeStationPos);
    SnapToTile(craftStationPos); SnapToTile(bossSpawnPos); SnapToTile(questBoardPos);
    for (int i = 0; i < 3; i++) SnapToTile(dungeonEntrances[i]);
}

// 床の周囲だけを壁(1)にし、無駄な空間(2)を描画・判定から除外する
void Dungeon::OptimizeMap() {
    for (int y = 0; y < currentHeight; y++) for (int x = 0; x < currentWidth; x++) {
        if (map[x][y] == 2) {
            bool nF = false; for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx; int ny = y + dy; if (nx >= 0 && nx < currentWidth && ny >= 0 && ny < currentHeight && map[nx][ny] == 0) nF = true;
            }
            if (nF) map[x][y] = 1; // 隣接するマスに床がある場合だけ壁にする
        }
    }
}

// 休憩フロア(5の倍数階)の生成
void Dungeon::GenerateRestFloor() {
    int cx = currentWidth / 2; int cy = currentHeight / 2;
    Room r1 = { cx - 5,cy + 5,10,10,RT_NORMAL }; Room r2 = { cx - 5,cy - 5,10,8,RT_HEAL }; Room r3 = { cx - 5,cy - 15,10,8,RT_NORMAL };
    rooms.push_back(r1); rooms.push_back(r2); rooms.push_back(r3);
    for (auto& r : rooms) for (int ry = r.y; ry < r.y + r.height; ry++) for (int rx = r.x; rx < r.x + r.width; rx++) map[rx][ry] = 0;
    DigCorridor(r1.GetCenter().x / TILE_SIZE, r1.GetCenter().z / TILE_SIZE, r2.GetCenter().x / TILE_SIZE, r2.GetCenter().z / TILE_SIZE);
    DigCorridor(r2.GetCenter().x / TILE_SIZE, r2.GetCenter().z / TILE_SIZE, r3.GetCenter().x / TILE_SIZE, r3.GetCenter().z / TILE_SIZE);
    stairsUpPos = r1.GetCenter(); stairsUpPos.y = 0.0f; portalPos = Vector3Add(stairsUpPos, { 3,0,0 });
    healStationPos = r2.GetCenter(); stairsDownPos = r3.GetCenter(); stairsDownPos.y = 0.0f;
}

// ボスフロア(10の倍数階)の生成
void Dungeon::GenerateBossFloor() {
    int cx = currentWidth / 2; int cy = currentHeight / 2;
    Room r1 = { cx - 6,cy + 10,12,8,RT_HEAL }; Room r2 = { cx - 15,cy - 15,30,20,RT_BOSS };
    rooms.push_back(r1); rooms.push_back(r2);
    for (auto& r : rooms) for (int ry = r.y; ry < r.y + r.height; ry++) for (int rx = r.x; rx < r.x + r.width; rx++) map[rx][ry] = 0;
    DigCorridor(r1.GetCenter().x / TILE_SIZE, r1.GetCenter().z / TILE_SIZE, r2.GetCenter().x / TILE_SIZE, r2.GetCenter().z / TILE_SIZE);
    stairsUpPos = r1.GetCenter(); stairsUpPos.y = 0.0f; healStationPos = Vector3Add(stairsUpPos, { -3,0,0 }); portalPos = Vector3Add(stairsUpPos, { 3,0,0 });
    bossSpawnPos = r2.GetCenter(); stairsDownPos = r2.GetCenter(); stairsDownPos.y = 0.0f;
}

// 通常フロアのランダム生成アルゴリズム
void Dungeon::GenerateNormalFloor(int floor) {
    // 部屋の種類のキューを用意（必ず各種類が1つは出るようにする）
    std::vector<RoomType> typeQueue = { RT_SMALL, RT_NORMAL, RT_LARGE, RT_TREASURE };
    int ex = 0; if (floor % 10 >= 1 && floor % 10 <= 4) ex = GetRandomValue(2, 3); else ex = GetRandomValue(3, 5);
    if (floor > 10) ex += (floor - 10) / 5; // 階層が深くなるほど部屋が増える

    int tR = 4 + ex; if (tR > 20) tR = 20; // 最大20部屋まで
    while ((int)typeQueue.size() < tR) { int r = GetRandomValue(0, 100); if (r < 30)typeQueue.push_back(RT_SMALL); else if (r < 60)typeQueue.push_back(RT_NORMAL); else if (r < 80)typeQueue.push_back(RT_LARGE); else typeQueue.push_back(RT_TREASURE); }

    for (auto t : typeQueue) {
        int w, h; switch (t) { case RT_SMALL:w = GetRandomValue(4, 6); h = w; break; case RT_NORMAL:w = GetRandomValue(7, 9); h = w; break; case RT_LARGE:w = GetRandomValue(12, 16); h = w; break; case RT_TREASURE:w = 4; h = 4; break; default:w = 5; h = 5; break; }
                                            // 部屋が重ならないようにランダム配置を試みる
                                            for (int k = 0; k < 50; k++) {
                                                int x = GetRandomValue(1, currentWidth - w - 1); int y = GetRandomValue(1, currentHeight - h - 1);
                                                bool ov = false; for (auto& r : rooms) if (x<r.x + r.width + 2 && x + w + 2>r.x && y<r.y + r.height + 2 && y + h + 2>r.y) { ov = true; break; }
                                                if (!ov) {
                                                    rooms.push_back({ x,y,w,h,t }); for (int ry = y; ry < y + h; ry++)for (int rx = x; rx < x + w; rx++)map[rx][ry] = 0;
                                                    // 1つ前の部屋と通路で繋ぐ
                                                    if (rooms.size() > 1) { Room& p = rooms[rooms.size() - 2]; DigCorridor(p.x + p.width / 2, p.y + p.height / 2, x + w / 2, y + h / 2); }
                                                    if (t == RT_TREASURE) treasureSpots.push_back({ (float)(x * TILE_SIZE + w * TILE_SIZE / 2),0.5f,(float)(y * TILE_SIZE + h * TILE_SIZE / 2) });
                                                    break;
                                                }
                                            }
    }
    // 上り階段と下り階段の配置
    std::vector<int> v; for (int i = 0; i < (int)rooms.size(); i++) if (rooms[i].type != RT_TREASURE) v.push_back(i);
    if (v.size() < 2) { v.clear(); for (int i = 0; i < (int)rooms.size(); i++)v.push_back(i); }
    if (v.size() >= 1) {
        stairsUpPos = rooms[v[0]].GetCenter(); stairsUpPos.y = 0.0f;
        stairsDownPos = rooms[v.size() > 1 ? v.back() : v[0]].GetCenter(); stairsDownPos.y = 0.0f;
    }
}

// 2点間を通路で繋ぐ（L字に掘る）
void Dungeon::DigCorridor(int x1, int y1, int x2, int y2) {
    x1 = (int)fmaxf(0, fminf((float)x1, (float)currentWidth - 1)); x2 = (int)fmaxf(0, fminf((float)x2, (float)currentWidth - 1));
    y1 = (int)fmaxf(0, fminf((float)y1, (float)currentHeight - 1)); y2 = (int)fmaxf(0, fminf((float)y2, (float)currentHeight - 1));
    for (int x = (int)fminf(x1, x2); x <= (int)fmaxf(x1, x2); x++) map[x][y1] = 0; // 横に掘る
    for (int y = (int)fminf(y1, y2); y <= (int)fmaxf(y1, y2); y++) map[x2][y] = 0; // 縦に掘る
}

// プレイヤーの現在位置をもとに未探索エリアの「霧(Fog of War)」を晴らす
void Dungeon::UpdateVisibility(Vector3 p) {
    if (isHome)return; int gx = (int)floorf((p.x + TILE_SIZE / 2) / TILE_SIZE); int gz = (int)floorf((p.z + TILE_SIZE / 2) / TILE_SIZE);
    for (int y = gz - 5; y <= gz + 5; y++)for (int x = gx - 5; x <= gx + 5; x++) if (x >= 0 && x < currentWidth && y >= 0 && y < currentHeight) discovered[x][y] = true;
}

// 3D空間にマップと環境オブジェクトを描画する
void Dungeon::Draw() {
    for (int y = 0; y < currentHeight; y++) for (int x = 0; x < currentWidth; x++) {
        if (!discovered[x][y]) continue; // 探索済みのマスのみ描画
        if (map[x][y] == 2) continue;
        Vector3 pos = { (float)x * TILE_SIZE, 0.0f, (float)y * TILE_SIZE };

        if (map[x][y] == 1) {
            DrawCube(pos, TILE_SIZE, 2.0f, TILE_SIZE, GRAY); // 壁
        }
        else {
            bool drawFloor = true;
            // 階段やポータルが配置されている床は、色を塗らない(チラつき防止)
            if (stairsDownPos.x != -999 && fabs(pos.x - stairsDownPos.x) < 0.1f && fabs(pos.z - stairsDownPos.z) < 0.1f) drawFloor = false;
            if (stairsUpPos.x != -999 && fabs(pos.x - stairsUpPos.x) < 0.1f && fabs(pos.z - stairsUpPos.z) < 0.1f) drawFloor = false;
            for (int i = 0; i < 3; i++) {
                if (dungeonEntrances[i].x != -999 && fabs(pos.x - dungeonEntrances[i].x) < 0.1f && fabs(pos.z - dungeonEntrances[i].z) < 0.1f) drawFloor = false;
            }

            if (drawFloor) {
                Color floorCol = DARKGREEN;
                if (isHome) floorCol = DARKBLUE;
                else if (currentDungeonId == 1) floorCol = MAROON;
                else if (currentDungeonId == 2) floorCol = DARKPURPLE;
                DrawPlane(pos, { TILE_SIZE, TILE_SIZE }, floorCol);
            }
        }
    }

    // 施設(3Dモデル)を描画するラムダ関数
    auto drawEnvTint = [](const std::string& key, Vector3 pos, Color tint) -> bool {
        if (DataManager::loadedModels.count(key) > 0) {
            DrawModel(DataManager::loadedModels[key].model, pos, 1.0f, tint);
            return true;
        }
        return false;
        };

    // 階段や施設を順番に描画(モデルが無ければ代替図形を描画)
    if (stairsDownPos.x != -999 && IsDiscovered(stairsDownPos.x, stairsDownPos.z)) {
        if (!drawEnvTint("Obj_StairsDown", stairsDownPos, WHITE)) DrawCube(stairsDownPos, 1.2f, 0.1f, 1.2f, GOLD);
    }
    if (stairsUpPos.x != -999 && IsDiscovered(stairsUpPos.x, stairsUpPos.z)) {
        if (!drawEnvTint("Obj_StairsUp", stairsUpPos, WHITE)) DrawCube(stairsUpPos, 1.2f, 0.1f, 1.2f, SKYBLUE);
    }
    if (isHome) {
        for (int i = 0; i < 3; i++) {
            if (dungeonEntrances[i].x != -999 && IsDiscovered(dungeonEntrances[i].x, dungeonEntrances[i].z)) {
                Color tint = WHITE; if (i == 1) tint = RED; if (i == 2) tint = PURPLE;
                if (!drawEnvTint("Obj_StairsDown", dungeonEntrances[i], tint)) DrawCube(dungeonEntrances[i], 1.2f, 0.1f, 1.2f, tint);
            }
        }
    }
    if (isHome && storageBoxPos.x != -999) {
        if (!drawEnvTint("Obj_Storage", storageBoxPos, WHITE)) { DrawCube(storageBoxPos, 1.2f, 1.2f, 1.2f, BROWN); }
    }
    if (isHome && reforgeStationPos.x != -999) {
        if (!drawEnvTint("Obj_Reforge", reforgeStationPos, WHITE)) { DrawCube(reforgeStationPos, 1.2f, 1.2f, 1.2f, PURPLE); }
    }
    if (isHome && craftStationPos.x != -999) {
        if (!drawEnvTint("Obj_Craft", craftStationPos, WHITE)) { DrawCube(craftStationPos, 1.2f, 1.2f, 1.2f, ORANGE); }
    }

    // 帰還ポータルの描画(回転して浮遊するアニメーション付き)
    if (portalPos.x != -999 && IsDiscovered(portalPos.x, portalPos.z)) {
        double time = GetTime();
        float yOffset = sin(time * 2.0f) * 0.5f;
        Vector3 drawPosition = portalPos; drawPosition.y += yOffset;
        float rotationAngle = time * 90.0f;
        Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };

        if (DataManager::loadedModels.count("Obj_Portal") > 0) {
            DrawModelEx(DataManager::loadedModels["Obj_Portal"].model, drawPosition, rotationAxis, rotationAngle, { 1.0f, 1.0f, 1.0f }, WHITE);
        }
        else {
            DrawCylinder(drawPosition, 1.0f, 1.0f, 2.5f, 8, Fade(PURPLE, 0.8f));
        }
    }

    if (healStationPos.x != -999 && IsDiscovered(healStationPos.x, healStationPos.z)) {
        if (!drawEnvTint("Obj_Heal", healStationPos, WHITE)) {
            DrawCube(healStationPos, 1.5f, 0.5f, 1.5f, PINK);
            Vector3 crossV = Vector3Add(healStationPos, { 0, 0.5f, 0 });
            DrawCube(crossV, 0.3f, 1.0f, 0.3f, RED); DrawCube(crossV, 1.0f, 0.3f, 0.3f, RED); // 十字マーク
        }
    }

    if (isHome && questBoardPos.x != -999) {
        if (!drawEnvTint("Obj_QuestBoard", questBoardPos, WHITE)) { DrawCube(questBoardPos, 2.0f, 2.0f, 0.2f, BEIGE); }
    }
}

// 座標が壁かどうかを判定
bool Dungeon::IsWall(float x, float z) {
    int gx = (int)floorf((x + TILE_SIZE / 2) / TILE_SIZE);
    int gz = (int)floorf((z + TILE_SIZE / 2) / TILE_SIZE);
    if (gx < 0 || gx >= currentWidth || gz < 0 || gz >= currentHeight) return true;
    return map[gx][gz] != 0;
}

// 円形の当たり判定と壁との衝突チェック(プレイヤーや敵の移動用)
bool Dungeon::CheckCollisionRadius(Vector3 p, float r) {
    int minX = (int)floorf((p.x - r + TILE_SIZE / 2) / TILE_SIZE);
    int maxX = (int)floorf((p.x + r + TILE_SIZE / 2) / TILE_SIZE);
    int minZ = (int)floorf((p.z - r + TILE_SIZE / 2) / TILE_SIZE);
    int maxZ = (int)floorf((p.z + r + TILE_SIZE / 2) / TILE_SIZE);

    for (int z = minZ; z <= maxZ; z++) {
        for (int x = minX; x <= maxX; x++) {
            if (x < 0 || x >= currentWidth || z < 0 || z >= currentHeight || map[x][z] != 0) {
                float cx = x * TILE_SIZE; float cz = z * TILE_SIZE; float halfT = TILE_SIZE / 2.0f;
                // 円の中心に最も近い矩形(壁)の座標を求める
                float closestX = fmaxf(cx - halfT, fminf(p.x, cx + halfT));
                float closestZ = fmaxf(cz - halfT, fminf(p.z, cz + halfT));
                float dx = p.x - closestX; float dz = p.z - closestZ;
                // 最短距離が半径より小さければ衝突
                if (dx * dx + dz * dz < r * r) return true;
            }
        }
    }
    return false;
}

// 2点間に壁がないか(射線が通るか)を判定する(敵の索敵や魔法用)
bool Dungeon::HasLineOfSight(Vector3 s, Vector3 e) { float d = Vector3Distance(s, e); Vector3 dir = Vector3Normalize(Vector3Subtract(e, s)); for (float t = 0.5f; t < d; t += 0.5f) { Vector3 p = Vector3Add(s, Vector3Scale(dir, t)); if (IsWall(p.x, p.z))return false; }return true; }
bool Dungeon::IsDiscovered(float x, float z) { int gx = (int)floorf((x + TILE_SIZE / 2) / TILE_SIZE); int gz = (int)floorf((z + TILE_SIZE / 2) / TILE_SIZE); return (gx >= 0 && gx < currentWidth && gz >= 0 && gz < currentHeight) ? discovered[gx][gz] : false; }
Vector3 Dungeon::GetStartPosition() { if (stairsUpPos.x != -999)return stairsUpPos; return rooms.empty() ? Vector3{ 0,0,0 } : rooms[0].GetCenter(); }

// フロア内の安全な(壁がない)ランダムな床座標を取得する(敵や宝箱のスポーン用)
Vector3 Dungeon::GetRandomFloorPos() {
    for (int i = 0; i < 50; i++) {
        int r = GetRandomValue(0, (int)rooms.size() - 1);
        if (rooms[r].type == RT_TREASURE || rooms[r].type == RT_HEAL || rooms[r].type == RT_BOSS)continue;

        int tx = GetRandomValue(rooms[r].x + 1, rooms[r].x + rooms[r].width - 2);
        int ty = GetRandomValue(rooms[r].y + 1, rooms[r].y + rooms[r].height - 2);
        Vector3 c = { (float)tx * TILE_SIZE + TILE_SIZE / 2.0f, 0.5f, (float)ty * TILE_SIZE + TILE_SIZE / 2.0f };

        // 階段の近くに敵がスポーンしないようにする(リスキル防止)
        if (stairsUpPos.x != -999 && Vector3Distance(c, stairsUpPos) < 8.0f)continue;
        if (stairsDownPos.x != -999 && Vector3Distance(c, stairsDownPos) < 5.0f)continue;

        if (!IsWall(c.x, c.z)) return c;
    }
    return{ 0,0,0 };
}