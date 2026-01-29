#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
#include "raymath.h"
#include <math.h>

int UI::itemPage = 0; int UI::equipPage = 0; int UI::itemSubTab = 0;

void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, bool debug, Font font) {
    for (auto& e : enemies) if (debug || d.IsDiscovered(e.position.x, e.position.z)) {
        Vector2 s = GetWorldToScreen(e.position, cam);
        DrawTextEx(font, TextFormat("Lv.%d %s", e.level, e.data.name.c_str()), Vector2{ s.x - 30.0f, s.y - 25.0f }, 16.0f, 1.0f, WHITE);
        DrawRectangle((int)s.x - 20, (int)s.y, 40, 4, DARKGRAY);
        DrawRectangle((int)s.x - 20, (int)s.y, (int)(40.0f * (e.hp / e.maxHp)), 4, RED);
    }
    int sH = GetScreenHeight(), sW = GetScreenWidth();
    DrawRectangle(10, sH - 120, 280, 110, Fade(BLACK, 0.6f));
    DrawTextEx(font, TextFormat("Lv: %d (EXP: %d/%d)", p.level, p.exp, p.expToNext), Vector2{ 20.0f, (float)sH - 110.0f }, 20.0f, 1.0f, SKYBLUE);
    DrawRectangle(20, sH - 85, 260, 18, DARKGRAY);
    DrawRectangle(20, sH - 85, (int)(260.0f * (fmaxf(0.0f, p.hp) / p.maxHp)), 18, GREEN);
    DrawTextEx(font, TextFormat("HP: %.0f/%.0f", p.hp, p.maxHp), Vector2{ 30.0f, (float)sH - 84.0f }, 14.0f, 1.0f, WHITE);

    std::string atkL = DataManager::uiStrings["ATK"];
    DrawTextEx(font, TextFormat("SP: %d  %s: %.1f", p.skillPoints, atkL.c_str(), p.attackPower + p.equippedData[p.activeSlot].atkBonus), Vector2{ 20.0f, (float)sH - 60.0f }, 18.0f, 1.0f, WHITE);

    DrawRectangle(sW - 220, sH - 140, 210, 130, Fade(BLACK, 0.6f));
    std::string fL = DataManager::uiStrings["FLOOR"];
    DrawTextEx(font, TextFormat("%s: %d", fL.c_str(), floor), Vector2{ (float)sW - 210.0f, (float)sH - 130.0f }, 22.0f, 1.0f, GOLD);
    DrawTextEx(font, TextFormat("[F1] DEBUG: %s", debug ? "ON" : "OFF"), Vector2{ (float)sW - 210.0f, (float)sH - 100.0f }, 16.0f, 1.0f, debug ? RED : GREEN);
    DrawTextEx(font, "[TAB] MENU", Vector2{ (float)sW - 210.0f, (float)sH - 45.0f }, 16.0f, 1.0f, RAYWHITE);

    int hX = GetScreenWidth() - 260, tc = 0;
    for (auto& e : enemies) if (e.hudTimer > 0) {
        int cY = 20 + (tc * 65); DrawRectangle(hX, cY, 240, 60, Fade(BLACK, 0.8f));
        DrawRectangleLines(hX, cY, 240, 60, (e.state == STATE_ATTACK ? RED : GRAY));
        DrawRectangle(hX + 10, cY + 35, 220, 12, DARKGRAY);
        DrawRectangle(hX + 10, cY + 35, (int)(220.0f * (e.hp / e.maxHp)), 12, RED);
        DrawTextEx(font, TextFormat("%s Lv.%d", e.data.name.c_str(), e.level), Vector2{ (float)hX + 10, (float)cY + 10 }, 16.0f, 1.0f, GOLD);
        tc++; if (tc >= 5) break;
    }
}

void UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab, Font font) {
    int sW = GetScreenWidth(); int sH = GetScreenHeight();
    DrawRectangle(100, 50, sW - 200, sH - 100, Fade(DARKGRAY, 0.95f));
    const char* tNames[] = { "EQUIP", "SKILL", "MAP", "ITEMS" };
    for (int i = 0; i < 4; i++) {
        Rectangle r = { 120.0f + (float)i * 145.0f, 70.0f, 135.0f, 40.0f };
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) tab = (MenuTab)i;
        DrawRectangleRec(r, (tab == i) ? BLUE : DARKGRAY);
        DrawTextEx(font, tNames[i], Vector2{ r.x + 35.0f, r.y + 10.0f }, 18.0f, 1.0f, WHITE);
    }
    if (tab == EQUIP) {
        for (int i = 0; i < 2; i++) {
            int y = 160 + i * 85; DrawRectangle(120, y, 260, 75, (p.activeSlot == i) ? MAROON : BLACK);
            DrawTextEx(font, p.equippedData[i].name.c_str(), Vector2{ 130, (float)y + 25 }, 18, 1.0f, WHITE);
        }
        const int perPage = 8; int maxP = (int)ceil((float)p.inventoryEquip.size() / perPage); if (maxP < 1) maxP = 1;
        for (int i = 0; i < perPage; i++) {
            int idx = equipPage * perPage + i; if (idx >= (int)p.inventoryEquip.size()) break;
            int y = 160 + i * 45; Rectangle r = { 420, (float)y, 300, 40 }; DrawRectangleRec(r, BLACK);
            DrawTextEx(font, p.inventoryEquip[idx].name.c_str(), Vector2{ 430, r.y + 10 }, 16, 1.0f, WHITE);
            Rectangle b1 = { 730, (float)y, 40, 40 }, b2 = { 775, (float)y, 40, 40 };
            if (CheckCollisionPointRec(GetMousePosition(), b1) && IsMouseButtonPressed(0)) p.EquipWeapon(idx, 0);
            if (CheckCollisionPointRec(GetMousePosition(), b2) && IsMouseButtonPressed(0)) p.EquipWeapon(idx, 1);
            DrawRectangleRec(b1, DARKGRAY); DrawRectangleRec(b2, DARKGRAY); DrawText("S1", (int)b1.x + 8, (int)b1.y + 10, 18, WHITE); DrawText("S2", (int)b2.x + 8, (int)b2.y + 10, 18, WHITE);
        }
    }
    else if (tab == INVENTORY) {
        const char* subNames[] = { "CONSUMABLE", "MATERIAL" };
        for (int i = 0; i < 2; i++) {
            Rectangle r = { 120.0f + i * 210, 120, 200, 35 }; if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { itemSubTab = i; itemPage = 0; }
            DrawRectangleRec(r, (itemSubTab == i) ? GREEN : BLACK); DrawTextEx(font, subNames[i], Vector2{ r.x + 10, r.y + 8 }, 16, 1.0f, WHITE);
        }
        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL";
        for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perPage = 10; int maxP = (int)ceil((float)filtered.size() / perPage); if (maxP < 1) maxP = 1;
        for (int i = 0; i < perPage; i++) {
            int lIdx = itemPage * perPage + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 165 + i * 42; DrawRectangle(120, y, 400, 38, Fade(BLACK, 0.4f)); DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), Vector2{ 135, (float)y + 10 }, 18, 1.0f, WHITE);
            if (itemSubTab == 0) { Rectangle b = { 530.0f, (float)y, 80.0f, 38.0f }; if (CheckCollisionPointRec(GetMousePosition(), b)) { DrawRectangleRec(b, GREEN); if (IsMouseButtonPressed(0)) p.UseItem(invIdx); } else DrawRectangleRec(b, DARKGREEN); DrawTextEx(font, "使う", Vector2{ b.x + 22.0f, b.y + 10.0f }, 18.0f, 1.0f, WHITE); }
        }
    }
    else if (tab == SKILL) {
        std::string sNames[] = { DataManager::uiStrings["SKILL_ATK"], DataManager::uiStrings["SKILL_DEF"], DataManager::uiStrings["SKILL_HP"] };
        for (int i = 0; i < 3; i++) {
            Rectangle r = { 120.0f, 180.0f + i * 65.0f, 450.0f, 55.0f };
            if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) p.UpgradeStat(i);
            DrawRectangleRec(r, (p.skillPoints > 0) ? GREEN : GRAY); DrawTextEx(font, sNames[i].c_str(), Vector2{ r.x + 20.0f, r.y + 18.0f }, 20.0f, 1.0f, BLACK);
        }
    }
    else if (tab == MAP) {
        float sc = 10.0f; float offX = (float)sW / 2.0f - (20 * sc), offY = (float)sH / 2.0f - (20 * sc);
        for (int y = 0; y < MAP_HEIGHT; y++) for (int x = 0; x < MAP_WIDTH; x++) if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) DrawRectangle((int)(offX + x * sc), (int)(offY + y * sc), (int)sc - 1, (int)sc - 1, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
        DrawCircle((int)(offX + (p.position.x / TILE_SIZE) * sc), (int)(offY + (p.position.z / TILE_SIZE) * sc), 4, RED);
    }
}

void UI::DrawStorage(Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLines(50, 50, sw - 100, sh - 100, GOLD);
    DrawTextEx(font, "STORAGE BOX", Vector2{ 70, 70 }, 24, 1.0f, GOLD);
    for (int i = 0; i < (int)p.inventoryItems.size(); i++) {
        Rectangle r = { 80, (float)150 + i * 35, 250, 30 }; DrawRectangleRec(r, DARKGRAY); DrawTextEx(font, p.inventoryItems[i].name.c_str(), Vector2{ r.x + 5, r.y + 5 }, 14, 1.0f, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) {
            bool found = false; for (auto& si : sItems) if (si.id == p.inventoryItems[i].id) { si.count += p.inventoryItems[i].count; found = true; break; }
            if (!found) sItems.push_back(p.inventoryItems[i]); p.inventoryItems.erase(p.inventoryItems.begin() + i); break;
        }
    }
    for (int i = 0; i < (int)sItems.size(); i++) {
        Rectangle r = { 400, (float)150 + i * 35, 250, 30 }; DrawRectangleRec(r, DARKBLUE);
        DrawTextEx(font, sItems[i].name.c_str(), Vector2{ r.x + 5, r.y + 5 }, 14, 1.0f, WHITE);
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { if (p.AddToInventory(sItems[i])) sItems.erase(sItems.begin() + i); break; }
    }
    Rectangle closeBtn = { (float)sw - 160, 70, 100, 40 }; DrawRectangleRec(closeBtn, RED);
    DrawTextEx(font, "CLOSE", Vector2{ closeBtn.x + 20, closeBtn.y + 10 }, 18, 1.0f, WHITE);
    if (CheckCollisionPointRec(GetMousePosition(), closeBtn) && IsMouseButtonPressed(0)) isOpen = false;
}

int UI::DrawPrompt(const char* label, int sw, int sh, Font font) {
    std::string msg = DataManager::uiStrings[label]; if (msg.empty()) msg = label;
    int bw = 450, bh = 180; int bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f)); DrawRectangleLines(bx, by, bw, bh, GOLD);
    Vector2 tSize = MeasureTextEx(font, msg.c_str(), 24.0f, 1.0f); DrawTextEx(font, msg.c_str(), Vector2{ (float)sw / 2.0f - tSize.x / 2.0f, (float)by + 40.0f }, 24.0f, 1.0f, WHITE);
    Rectangle bY = { (float)sw / 2.0f - 140.0f, (float)by + 100.0f, 120.0f, 50.0f }, bN = { (float)sw / 2.0f + 20.0f, (float)by + 100.0f, 120.0f, 50.0f };

    int result = 0; // ここを明示的に初期化！
    if (CheckCollisionPointRec(GetMousePosition(), bY)) { DrawRectangleRec(bY, GREEN); if (IsMouseButtonPressed(0)) result = 1; }
    else DrawRectangleRec(bY, DARKGRAY);
    if (CheckCollisionPointRec(GetMousePosition(), bN)) { DrawRectangleRec(bN, RED); if (IsMouseButtonPressed(0)) result = 2; }
    else DrawRectangleRec(bN, DARKGRAY);

    DrawTextEx(font, DataManager::uiStrings["YES"].c_str(), Vector2{ bY.x + 40.0f, bY.y + 15.0f }, 20.0f, 1.0f, WHITE);
    DrawTextEx(font, DataManager::uiStrings["NO"].c_str(), Vector2{ bN.x + 40.0f, bN.y + 15.0f }, 20.0f, 1.0f, WHITE);
    return result;
}

void UI::DrawLogs(std::vector<GameLog>& logs, Font font) {
    for (int i = 0; i < (int)logs.size(); i++) {
        float a = fminf(1.0f, logs[i].life * 2.0f);
        DrawTextEx(font, logs[i].message.c_str(), Vector2{ 20.0f, 20.0f + (float)i * 25.0f }, 20.0f, 1.0f, Fade(logs[i].color, a));
    }
}

void UI::DrawNearbyItems(Player& p, std::vector<DroppedItem>& di, Camera3D& cam, Font font) {
    for (auto& item : di) {
        float d = Vector3Distance(p.position, item.pos);
        if (d < 5.0f) {
            Vector2 s = GetWorldToScreen(item.pos, cam);
            DrawTextEx(font, item.data.name.c_str(), Vector2{ s.x - 20, s.y - 20 }, 16.0f, 1.0f, LIME);
            if (d < 1.5f) DrawTextEx(font, "PICK UP!", Vector2{ s.x - 20, s.y + 5 }, 14.0f, 1.0f, YELLOW);
        }
    }
}