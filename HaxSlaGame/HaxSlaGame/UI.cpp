#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
#include <math.h>

void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, bool debug) {
    for (auto& e : enemies) if (debug || d.IsDiscovered(e.position.x, e.position.z)) {
        Vector2 s = GetWorldToScreen(e.position, cam);
        DrawText(TextFormat("Lv.%d %s", e.level, e.data.name.c_str()), (int)s.x - 30, (int)s.y - 25, 10, WHITE);
        DrawRectangle((int)s.x - 20, (int)s.y, 40, 4, DARKGRAY);
        DrawRectangle((int)s.x - 20, (int)s.y, (int)(40.0f * (e.hp / e.maxHp)), 4, RED);
    }
    int sH = GetScreenHeight(); int sW = GetScreenWidth();
    DrawRectangle(10, sH - 120, 280, 110, Fade(BLACK, 0.6f));
    DrawText(TextFormat("Lv: %d (EXP: %d/%d)", p.level, p.exp, p.expToNext), 20, sH - 110, 20, SKYBLUE);
    DrawRectangle(20, sH - 85, 260, 18, DARKGRAY);
    DrawRectangle(20, sH - 85, (int)(260.0f * (fmaxf(0.0f, p.hp) / p.maxHp)), 18, GREEN);
    DrawText(TextFormat("HP: %.0f/%.0f", p.hp, p.maxHp), 30, sH - 84, 14, WHITE);
    DrawText(TextFormat("SP: %d  ATK: %.1f", p.skillPoints, p.attackPower), 20, sH - 60, 18, WHITE);

    DrawRectangle(sW - 220, sH - 110, 210, 100, Fade(BLACK, 0.6f));
    DrawText(TextFormat("FLOOR: %d", floor), sW - 210, sH - 100, 22, GOLD);
    DrawText(TextFormat("[F1] DEBUG: %s", debug ? "ON" : "OFF"), sW - 210, sH - 70, 16, debug ? RED : GREEN);
    DrawText("[TAB] MENU", sW - 210, sH - 45, 16, RAYWHITE);
}

void UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab) {
    int sW = GetScreenWidth(); int sH = GetScreenHeight();
    DrawRectangle(100, 50, sW - 200, sH - 100, Fade(DARKGRAY, 0.95f));
    const char* tNames[] = { "EQUIP", "SKILL", "MAP", "ITEMS" };
    for (int i = 0; i < 4; i++) {
        Rectangle r = { (float)120 + i * 145, 70.0f, 135.0f, 40.0f };
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) tab = (MenuTab)i;
        DrawRectangleRec(r, (tab == i) ? BLUE : DARKGRAY);
        DrawText(tNames[i], (int)r.x + 35, (int)r.y + 10, 20, WHITE);
    }
    if (tab == INVENTORY) {
        for (int i = 0; i < (int)p.inventory.size(); i++) {
            int y = 175 + i * 50; DrawRectangle(120, y, 300, 45, Fade(BLACK, 0.3f));
            DrawText(TextFormat("%s x%d", p.inventory[i].name.c_str(), p.inventory[i].count), 135, y + 12, 20, WHITE);
            if (p.inventory[i].type == "CONSUMABLE") {
                Rectangle btn = { 530.0f, (float)y, 100.0f, 45.0f };
                if (CheckCollisionPointRec(GetMousePosition(), btn)) { DrawRectangleRec(btn, GREEN); if (IsMouseButtonPressed(0)) p.UseItem(i); }
                else DrawRectangleRec(btn, DARKGREEN);
                DrawText("USE", (int)btn.x + 30, (int)btn.y + 12, 18, WHITE);
            }
            else if (p.inventory[i].type == "EQUIP") {
                Rectangle b1 = { 530.0f, (float)y, 60.0f, 45.0f }, b2 = { 600.0f, (float)y, 60.0f, 45.0f };
                if (CheckCollisionPointRec(GetMousePosition(), b1)) { DrawRectangleRec(b1, RED); if (IsMouseButtonPressed(0)) p.EquipWeapon(i, 0); }
                else DrawRectangleRec(b1, MAROON);
                if (CheckCollisionPointRec(GetMousePosition(), b2)) { DrawRectangleRec(b2, RED); if (IsMouseButtonPressed(0)) p.EquipWeapon(i, 1); }
                else DrawRectangleRec(b2, MAROON);
                DrawText("S1", (int)b1.x + 18, (int)b1.y + 12, 20, WHITE); DrawText("S2", (int)b2.x + 18, (int)b2.y + 12, 20, WHITE);
            }
        }
    }
    else if (tab == SKILL) {
        const char* sk[] = { "ATK +5", "DEF +3", "MAX HP +50" };
        for (int i = 0; i < 3; i++) {
            Rectangle r = { 120.0f, 180.0f + i * 65.0f, 450.0f, 55.0f };
            if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) p.UpgradeStat(i);
            DrawRectangleRec(r, (p.skillPoints > 0) ? GREEN : GRAY);
            DrawText(sk[i], (int)r.x + 20, (int)r.y + 18, 20, BLACK);
        }
    }
    else if (tab == MAP) {
        float sc = 10.0f; float offX = (float)sW / 2.0f - (20 * sc), offY = (float)sH / 2.0f - (20 * sc);
        for (int y = 0; y < MAP_HEIGHT; y++) for (int x = 0; x < MAP_WIDTH; x++)
            if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE))
                DrawRectangle((int)(offX + x * sc), (int)(offY + y * sc), (int)sc - 1, (int)sc - 1, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
        DrawCircle((int)(offX + (p.position.x / TILE_SIZE) * sc), (int)(offY + (p.position.z / TILE_SIZE) * sc), 4, RED);
    }
}

int UI::DrawPrompt(const char* msg, int sw, int sh) {
    int bw = 450, bh = 180; int bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f)); DrawRectangleLines(bx, by, bw, bh, GOLD);
    DrawText(msg, sw / 2 - MeasureText(msg, 24) / 2, by + 40, 24, WHITE);
    Rectangle bY = { (float)sw / 2 - 140.0f, (float)by + 100.0f, 120.0f, 50.0f }, bN = { (float)sw / 2 + 20.0f, (float)by + 100.0f, 120.0f, 50.0f };
    int res = 0;
    if (CheckCollisionPointRec(GetMousePosition(), bY)) { DrawRectangleRec(bY, GREEN); if (IsMouseButtonPressed(0)) res = 1; }
    else DrawRectangleRec(bY, DARKGRAY);
    if (CheckCollisionPointRec(GetMousePosition(), bN)) { DrawRectangleRec(bN, RED); if (IsMouseButtonPressed(0)) res = 2; }
    else DrawRectangleRec(bN, DARKGRAY);
    DrawText("YES", (int)bY.x + 40, (int)bY.y + 15, 20, WHITE); DrawText("NO", (int)bN.x + 40, (int)bN.y + 15, 20, WHITE);
    return res;
}