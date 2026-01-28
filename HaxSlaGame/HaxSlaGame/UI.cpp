#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"

void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, bool debug) {
    for (auto& e : enemies) if (debug || d.IsDiscovered(e.position.x, e.position.z)) {
        Vector2 s = GetWorldToScreen({ e.position.x, 1.8f, e.position.z }, cam);
        DrawText(TextFormat("Lv.%d", e.level), (int)s.x - 15, (int)s.y - 15, 12, WHITE);
        DrawRectangle((int)s.x - 20, (int)s.y, 40, 4, DARKGRAY);
        DrawRectangle((int)s.x - 20, (int)s.y, (int)(40.0f * (e.hp / e.maxHp)), 4, RED);
    }
    int sH = GetScreenHeight(), sW = GetScreenWidth();
    DrawRectangle(10, sH - 95, 260, 85, Fade(BLACK, 0.5f));
    DrawText(TextFormat("Lv: %d (EXP: %d/%d)", p.level, p.exp, p.expToNext), 20, sH - 90, 18, SKYBLUE);
    DrawRectangle(20, sH - 65, (int)(240.0f * (fmaxf(0, p.hp) / p.maxHp)), 15, GREEN);
    DrawText(TextFormat("SP: %d  ATK: %.1f", p.skillPoints, p.attackPower), 20, sH - 38, 16, WHITE);
    DrawRectangle(sW - 220, sH - 110, 210, 100, Fade(BLACK, 0.5f));
    DrawText(TextFormat("Floor: %d", floor), sW - 210, sH - 100, 18, GOLD);
    DrawText(TextFormat("[F1] DEBUG: %s", debug ? "ON" : "OFF"), sW - 210, sH - 75, 16, debug ? RED : GREEN);
    DrawText("[TAB] MENU", sW - 210, sH - 50, 16, RAYWHITE);
}
void UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab) {
    int sW = GetScreenWidth(), sH = GetScreenHeight();
    DrawRectangle(100, 50, sW - 200, sH - 100, Fade(DARKGRAY, 0.9f));
    const char* tNames[] = { "EQUIP", "SKILLS", "MAP", "INVENTORY" };
    for (int i = 0; i < 4; i++) {
        Rectangle r = { (float)120 + i * 140, 70, 130, 40 };
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) tab = (MenuTab)i;
        DrawRectangleRec(r, (tab == i) ? BLUE : DARKGRAY); DrawText(tNames[i], (int)r.x + 10, (int)r.y + 10, 18, WHITE);
    }
    if (tab == EQUIP) {
        const char* wn[] = { "SWORD", "SPEAR", "AXE", "BOW", "WAND" };
        for (int i = 0; i < 5; i++) {
            Rectangle r1 = { 120, 170.0f + i * 50, 150, 40 }, r2 = { 280, 170.0f + i * 50, 150, 40 };
            if (CheckCollisionPointRec(GetMousePosition(), r1) && IsMouseButtonPressed(0)) p.equippedWeapons[0] = (WeaponType)i;
            if (CheckCollisionPointRec(GetMousePosition(), r2) && IsMouseButtonPressed(0)) p.equippedWeapons[1] = (WeaponType)i;
            DrawRectangleRec(r1, (p.equippedWeapons[0] == i) ? MAROON : GRAY); DrawRectangleRec(r2, (p.equippedWeapons[1] == i) ? MAROON : GRAY);
            DrawText(wn[i], (int)r1.x + 10, (int)r1.y + 10, 18, WHITE);
        }
    }
    else if (tab == SKILL) {
        const char* sk[] = { "ATK +5", "DEF +3", "MAX HP +50" };
        for (int i = 0; i < 3; i++) {
            Rectangle r = { 120, 180.0f + i * 60, 400, 50 };
            if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) p.UpgradeStat(i);
            DrawRectangleRec(r, (p.skillPoints > 0) ? GREEN : GRAY); DrawText(sk[i], (int)r.x + 20, (int)r.y + 15, 20, BLACK);
        }
    }
    else if (tab == MAP) {
        float sc = 10.0f;
        for (int y = 0; y < MAP_HEIGHT; y++) for (int x = 0; x < MAP_WIDTH; x++)
            if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) DrawRectangle(450 + x * (int)sc, 180 + y * (int)sc, (int)sc, (int)sc, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
        DrawCircle(450 + (int)((p.position.x / TILE_SIZE) * sc), 180 + (int)((p.position.z / TILE_SIZE) * sc), 4, RED);
    }
    else if (tab == INVENTORY) {
        for (int i = 0; i < (int)p.inventory.size(); i++) {
            int y = 170 + i * 45; DrawRectangle(120, y, 300, 40, Fade(BLACK, 0.4f)); DrawText(TextFormat("%s x%d", p.inventory[i].name.c_str(), p.inventory[i].count), 130, y + 10, 18, WHITE);
            if (p.inventory[i].type == "CONSUMABLE") { Rectangle b = { 430, (float)y, 80, 40 }; if (CheckCollisionPointRec(GetMousePosition(), b) && IsMouseButtonPressed(0)) p.UseItem(i); DrawRectangleRec(b, GREEN); DrawText("USE", (int)b.x + 20, (int)b.y + 10, 18, WHITE); }
            else if (p.inventory[i].type == "EQUIP") { Rectangle b1 = { 430, (float)y, 50, 40 }, b2 = { 490, (float)y, 50, 40 }; if (CheckCollisionPointRec(GetMousePosition(), b1) && IsMouseButtonPressed(0)) p.EquipWeapon(i, 0); if (CheckCollisionPointRec(GetMousePosition(), b2) && IsMouseButtonPressed(0)) p.EquipWeapon(i, 1); DrawRectangleRec(b1, MAROON); DrawRectangleRec(b2, MAROON); DrawText("S1", (int)b1.x + 10, (int)b1.y + 10, 18, WHITE); DrawText("S2", (int)b2.x + 10, (int)b2.y + 10, 18, WHITE); }
        }
    }
}
int UI::DrawPrompt(const char* msg, int sw, int sh) {
    int bw = 400, bh = 160; int bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f)); DrawRectangleLines(bx, by, bw, bh, GOLD);
    int tw = MeasureText(msg, 20); DrawText(msg, sw / 2 - tw / 2, by + 40, 20, RAYWHITE);
    Rectangle bY = { (float)sw / 2 - 130, (float)by + 90, 120, 40 }, bN = { (float)sw / 2 + 10, (float)by + 90, 120, 40 };
    if (CheckCollisionPointRec(GetMousePosition(), bY)) { DrawRectangleRec(bY, GREEN); if (IsMouseButtonPressed(0)) return 1; }
    else DrawRectangleRec(bY, DARKGRAY);
    if (CheckCollisionPointRec(GetMousePosition(), bN)) { DrawRectangleRec(bN, MAROON); if (IsMouseButtonPressed(0)) return 2; }
    else DrawRectangleRec(bN, DARKGRAY);
    DrawText("YES", (int)bY.x + 35, (int)bY.y + 10, 20, WHITE); DrawText("NO", (int)bN.x + 45, (int)bN.y + 10, 20, WHITE);
    return 0;
}