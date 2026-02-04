#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
#include "raymath.h"
#include <math.h>

int UI::itemPage = 0; int UI::equipPage = 0; int UI::debugPage = 0;
int UI::storageInvPage = 0; int UI::storageBoxPage = 0; int UI::itemSubTab = 0;

bool UI::DrawButton(Rectangle r, const char* label, Font font, Color col) {
    bool clicked = false; bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hover ? ColorBrightness(col, 0.2f) : col);
    DrawRectangleLinesEx(r, 2, RAYWHITE);
    Vector2 tSize = MeasureTextEx(font, label, 18, 1);
    DrawTextEx(font, label, { r.x + r.width / 2 - tSize.x / 2, r.y + r.height / 2 - tSize.y / 2 }, 18, 1, WHITE);
    if (hover && IsMouseButtonPressed(0)) clicked = true;
    return clicked;
}

void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, bool debug, Font font) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    // 敵HPバー（リスト）
    int listCount = 0;
    for (auto& e : enemies) {
        if (e.hudTimer > 0) {
            int yPos = 20 + listCount * 50;
            DrawRectangle(sw - 220, yPos, 200, 45, Fade(BLACK, 0.7f));
            DrawTextEx(font, TextFormat("Lv.%d %s", e.level, e.data.name.c_str()), { (float)sw - 210, (float)yPos + 5 }, 16, 1, WHITE);
            DrawRectangle(sw - 210, yPos + 25, 180, 10, DARKGRAY);
            DrawRectangle(sw - 210, yPos + 25, (int)(180.0f * (e.hp / e.maxHp)), 10, RED);
            listCount++; if (listCount >= 5) break;
        }
        // 頭上HPバー
        if (debug || d.IsDiscovered(e.position.x, e.position.z)) {
            Vector2 s = GetWorldToScreen(e.position, cam);
            if (s.x > 0 && s.y > 0 && s.x < sw && s.y < sh) {
                std::string txt = "Lv." + std::to_string(e.level) + " " + e.data.name;
                Vector2 tSize = MeasureTextEx(font, txt.c_str(), 16, 1);
                DrawTextEx(font, txt.c_str(), { s.x - tSize.x / 2, s.y - 45 }, 16, 1, WHITE);
                DrawRectangle((int)s.x - 20, (int)s.y - 25, 40, 4, DARKGRAY);
                DrawRectangle((int)s.x - 20, (int)s.y - 25, (int)(40.0f * (e.hp / e.maxHp)), 4, RED);
            }
        }
    }

    // プレイヤー情報
    DrawRectangle(10, sh - 120, 320, 110, Fade(BLACK, 0.6f));
    DrawTextEx(font, TextFormat("Lv: %d   EXP: %d/%d", p.level, p.exp, p.expToNext), { 20, (float)sh - 110 }, 18, 1, SKYBLUE);
    DrawRectangle(20, sh - 85, 280, 18, DARKGRAY);
    DrawRectangle(20, sh - 85, (int)(260 * (fmaxf(0.0f, p.hp) / p.maxHp)), 18, GREEN);
    DrawTextEx(font, TextFormat("HP: %.0f/%.0f", p.hp, p.maxHp), { 30, (float)sh - 84 }, 14, 1, WHITE);
    DrawTextEx(font, TextFormat("SP: %d   ATK: %.1f", p.skillPoints, p.attackPower + p.equippedData[p.activeSlot].atkBonus), { 20, (float)sh - 60 }, 18, 1, WHITE);
}

void UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab, Font font) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(100, 50, sw - 200, sh - 100, Fade(DARKGRAY, 0.95f));
    const char* tKeys[] = { "EQUIP", "SKILL", "MAP", "ITEMS", "DEBUG" };

    // タブ切り替え
    for (int i = 0; i < 5; i++) {
        Rectangle r = { 110.0f + i * 135, 70.0f, 130.0f, 40.0f };
        if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { tab = (MenuTab)i; }
        DrawRectangleRec(r, (tab == i) ? BLUE : DARKGRAY);
        std::string label = DataManager::uiStrings[tKeys[i]]; if (label.empty()) label = tKeys[i];
        Vector2 lSize = MeasureTextEx(font, label.c_str(), 20, 1);
        DrawTextEx(font, label.c_str(), { r.x + (r.width / 2 - lSize.x / 2), r.y + 10 }, 18, 1, WHITE);
    }

    // 内容描画
    if (tab == EQUIP) {
        DrawTextEx(font, DataManager::uiStrings["ACTIVE_SLOTS"].c_str(), { 120, 130 }, 20, 1, GOLD);
        for (int i = 0; i < 2; i++) {
            int y = 160 + i * 105; bool isEmpty = (p.equippedData[i].id == -1);
            DrawRectangle(120, y, 260, 95, (p.activeSlot == i) ? MAROON : BLACK);
            if (!isEmpty) {
                DrawTextEx(font, p.equippedData[i].name.c_str(), { 130, (float)y + 25 }, 20, 1, WHITE);
                DrawTextEx(font, TextFormat("%s +%.0f", DataManager::uiStrings["ATK"].c_str(), p.equippedData[i].atkBonus), { 130, (float)y + 50 }, 14, 1, YELLOW);
                if (UI::DrawButton({ 310, (float)y + 25, 60, 40 }, "はずす", font, RED)) p.UnequipWeapon(i);
            }
            else DrawTextEx(font, "EMPTY", { 130, (float)y + 35 }, 20, 1, DARKGRAY);
        }
        // インベントリ(装備)
        const int perP = 8; int maxP = (int)ceil((float)p.inventoryEquip.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(DataManager::uiStrings["PAGE_INFO"].c_str(), equipPage + 1, maxP), { 420, 130 }, 18, 1, GOLD);
        for (int i = 0; i < perP; i++) {
            int idx = equipPage * perP + i; if (idx >= (int)p.inventoryEquip.size()) break;
            int y = 160 + i * 45; Rectangle r = { 420, (float)y, 250, 40 }; DrawRectangleRec(r, BLACK);
            DrawTextEx(font, p.inventoryEquip[idx].name.c_str(), { 430, (float)y + 10 }, 16, 1, WHITE);
            if (UI::DrawButton({ 680, (float)y, 45, 40 }, "S1", font, DARKGRAY)) p.EquipWeapon(idx, 0);
            if (UI::DrawButton({ 730, (float)y, 45, 40 }, "S2", font, DARKGRAY)) p.EquipWeapon(idx, 1);
        }
        if (UI::DrawButton({ 420, 530, 80, 30 }, "<<", font, GRAY) && equipPage > 0) equipPage--;
        if (UI::DrawButton({ 510, 530, 80, 30 }, ">>", font, GRAY) && equipPage < maxP - 1) equipPage++;
    }
    else if (tab == INVENTORY) {
        const char* subK[] = { "CONSUMABLE", "MATERIAL" };
        for (int i = 0; i < 2; i++) {
            Rectangle r = { 120.0f + i * 210, 120, 200, 35 }; if (CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { itemSubTab = i; itemPage = 0; }
            DrawRectangleRec(r, (itemSubTab == i) ? GREEN : BLACK);
            std::string label = DataManager::uiStrings[subK[i]]; if (label.empty()) label = subK[i];
            DrawTextEx(font, label.c_str(), { r.x + 10, r.y + 8 }, 16, 1, WHITE);
        }
        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL";
        for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perP = 10; int maxP = (int)ceil((float)filtered.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(DataManager::uiStrings["PAGE_INFO"].c_str(), itemPage + 1, maxP), { 600, 125 }, 18, 1, WHITE);
        for (int i = 0; i < perP; i++) {
            int lIdx = itemPage * perP + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 165 + i * 42; DrawRectangle(120, y, 400, 38, Fade(BLACK, 0.4f));
            DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), { 135, (float)y + 10 }, 18, 1.0f, WHITE);
            if (itemSubTab == 0 && UI::DrawButton({ 530, (float)y, 80, 38 }, "使う", font, GREEN)) p.UseItem(invIdx);
        }
        if (UI::DrawButton({ 120, 600, 100, 30 }, "<<", font, GRAY) && itemPage > 0) itemPage--;
        if (UI::DrawButton({ 230, 600, 100, 30 }, ">>", font, GRAY) && itemPage < maxP - 1) itemPage++;
    }
    else if (tab == SKILL) {
        for (auto& node : p.skillTree) for (int reqId : node.reqIds) DrawLineEx(node.uiPos, p.skillTree[reqId].uiPos, 3, node.unlocked ? GOLD : DARKGRAY);
        for (int i = 0; i < (int)p.skillTree.size(); i++) {
            auto& node = p.skillTree[i]; bool available = p.IsSkillAvailable(i);
            DrawPoly(node.uiPos, 6, 35, 0, node.unlocked ? YELLOW : (available ? GREEN : DARKGRAY));
            DrawPolyLines(node.uiPos, 6, 35, 0, RAYWHITE);
            DrawTextEx(font, node.name.c_str(), { node.uiPos.x - 28, node.uiPos.y - 8 }, 12, 1, node.unlocked ? BLACK : WHITE);
            if (available && CheckCollisionPointCircle(GetMousePosition(), node.uiPos, 35) && IsMouseButtonPressed(0)) p.UnlockSkill(i);
        }
    }
    else if (tab == DEBUG_TAB) {
        const int perP = 10; int total = (int)DataManager::itemConfigs.size(); int maxP = (int)ceil((float)total / perP);
        for (int i = 0; i < perP; i++) {
            int idx = debugPage * perP + i; if (idx >= total) break;
            auto& cfg = DataManager::itemConfigs[idx]; int y = 160 + i * 42;
            DrawRectangle(120, y, 450, 38, Fade(BLACK, 0.5f)); DrawTextEx(font, TextFormat("[%s] %s", cfg.type.c_str(), cfg.name.c_str()), { 130, (float)y + 10 }, 16, 1, WHITE);
            if (UI::DrawButton({ 580, (float)y, 60, 38 }, "GET", font, RED)) p.AddToInventory(cfg);
        }
        if (UI::DrawButton({ 120, 600, 80, 30 }, "<<", font, GRAY) && debugPage > 0) debugPage--;
        if (UI::DrawButton({ 210, 600, 80, 30 }, ">>", font, GRAY) && debugPage < maxP - 1) debugPage++;
        if (UI::DrawButton({ 680, 160, 180, 50 }, "SP +999", font, BLUE)) p.skillPoints += 999;
    }
    else if (tab == MAP_TAB) {
        float sc = 12.0f; float offX = (sw / 2.0f) - (20 * sc); float offY = (sh / 2.0f) - (20 * sc) + 40;
        DrawRectangle(offX - 5, offY - 5, 40 * sc + 10, 40 * sc + 10, BLACK);
        for (int y = 0; y < 40; y++) for (int x = 0; x < 40; x++) if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) DrawRectangle(offX + x * sc, offY + y * sc, sc - 1, sc - 1, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
        DrawCircle(offX + (p.position.x / TILE_SIZE) * sc, offY + (p.position.z / TILE_SIZE) * sc, 5, RED);
    }
}

void UI::DrawStorage(Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, GOLD);
    DrawTextEx(font, "手持ちアイテム", { 80, 120 }, 20, 1, SKYBLUE);
    const int perP = 10; int mPInv = (int)ceil((float)p.inventoryItems.size() / perP); int mPBox = (int)ceil((float)sItems.size() / perP); if (mPInv < 1)mPInv = 1; if (mPBox < 1)mPBox = 1;
    for (int i = 0; i < perP; i++) {
        int idx = storageInvPage * perP + i; if (idx >= (int)p.inventoryItems.size()) break;
        Rectangle r = { 80, (float)170 + i * 42, 350, 38 }; DrawRectangleRec(r, DARKGRAY);
        DrawTextEx(font, TextFormat("%s x%d", p.inventoryItems[idx].name.c_str(), p.inventoryItems[idx].count), { r.x + 10, r.y + 10 }, 18, 1, WHITE);
        if (UI::DrawButton({ 440, r.y, 110, 38 }, "預ける >>", font, BLUE)) {
            bool f = false; for (auto& si : sItems) if (si.id == p.inventoryItems[idx].id) { si.count += p.inventoryItems[idx].count; f = true; break; }
            if (!f) sItems.push_back(p.inventoryItems[idx]); p.inventoryItems.erase(p.inventoryItems.begin() + idx); break;
        }
    }
    if (UI::DrawButton({ 80, 600, 100, 30 }, "<<", font, GRAY) && storageInvPage > 0) storageInvPage--;
    if (UI::DrawButton({ 190, 600, 100, 30 }, ">>", font, GRAY) && storageInvPage < mPInv - 1) storageInvPage++;

    DrawTextEx(font, "倉庫のアイテム", { 620, 120 }, 20, 1, GREEN);
    for (int i = 0; i < perP; i++) {
        int idx = storageBoxPage * perP + i; if (idx >= (int)sItems.size()) break;
        Rectangle r = { 770, (float)170 + i * 42, 350, 38 }; DrawRectangleRec(r, DARKBLUE);
        DrawTextEx(font, TextFormat("%s x%d", sItems[idx].name.c_str(), sItems[idx].count), { r.x + 10, r.y + 10 }, 18, 1, WHITE);
        if (UI::DrawButton({ 620, r.y, 110, 38 }, "<< 取出す", font, DARKGREEN)) if (p.AddToInventory(sItems[idx])) sItems.erase(sItems.begin() + idx);
    }
    if (UI::DrawButton({ 620, 600, 100, 30 }, "<<", font, GRAY) && storageBoxPage > 0) storageBoxPage--;
    if (UI::DrawButton({ 730, 600, 100, 30 }, ">>", font, GRAY) && storageBoxPage < mPBox - 1) storageBoxPage++;

    if (UI::DrawButton({ (float)sw - 160, 70, 100, 45 }, "閉じる", font, RED)) isOpen = false;
}

void UI::DrawLogs(std::vector<GameLog>& logs, Font font) {
    for (int i = 0; i < (int)logs.size(); i++) {
        float a = fminf(1.0f, logs[i].life * 2.0f);
        DrawTextEx(font, logs[i].message.c_str(), { 20, 20 + (float)i * 25 }, 20, 1, Fade(logs[i].color, a));
    }
}

void UI::DrawNearbyItems(Player& p, std::vector<DroppedItem>& di, Camera3D& cam, Font font) {
    for (auto& item : di) {
        float d = Vector3Distance(p.position, item.pos);
        if (d < 5.0f) {
            Vector2 s = GetWorldToScreen(item.pos, cam);
            if (s.x > 0 && s.y > 0) {
                DrawTextEx(font, item.data.name.c_str(), { s.x - 20, s.y - 20 }, 16, 1, LIME);
                if (d < 1.5f) DrawTextEx(font, "[CLICK] PICK UP", { s.x - 40, s.y + 5 }, 14, 1, YELLOW);
            }
        }
    }
}

int UI::DrawPrompt(const char* label, int sw, int sh, Font font) {
    std::string msg = DataManager::uiStrings[label]; if (msg.empty()) msg = label;
    int bw = 450, bh = 180, bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f)); DrawRectangleLines(bx, by, bw, bh, GOLD);
    Vector2 tS = MeasureTextEx(font, msg.c_str(), 24, 1); DrawTextEx(font, msg.c_str(), { (float)sw / 2 - tS.x / 2, (float)by + 40 }, 24, 1, WHITE);
    Rectangle bY = { (float)sw / 2 - 140, (float)by + 100, 120, 50 }, bN = { (float)sw / 2 + 20, (float)by + 100, 120, 50 };
    int res = 0;
    if (UI::DrawButton(bY, "YES", font, GREEN)) res = 1;
    if (UI::DrawButton(bN, "NO", font, RED)) res = 2;
    return res;
}