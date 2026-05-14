#include "UI.h"
#include "Player.h"
#include "Dungeon.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "raymath.h"
#include <math.h>

static std::string T(const std::string& key, const std::string& def) {
    if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
    return def;
}

int UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab, Font font) {
    int eventCode = 0;
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(100, 50, sw - 200, sh - 100, Fade(DARKGRAY, 0.95f));
    const char* tKeys[] = { "EQUIP", "SKILL", "MAP", "ITEMS", "DEBUG", "SYSTEM", "OPTION" };
    const char* tDefs[] = { "Equip", "Skill", "Map", "Items", "Debug", "System", "Option" };

    for (int i = 0; i < 7; i++) {
        Rectangle r = { 110.0f + (float)i * 135, 70.0f, 130.0f, 40.0f };
        Color tabColor = (tab == i) ? BLUE : DARKGRAY;
        if (UI::DrawButton(r, T(tKeys[i], tDefs[i]).c_str(), font, tabColor)) { tab = (MenuTab)i; }
    }

    if (tab == EQUIP) {
        DrawTextEx(font, T("ACTIVE_SLOTS", "Equipped").c_str(), { 120, 130 }, 20, 1, GOLD);
        for (int i = 0; i < 2; i++) {
            int y = 160 + i * 105; bool isEmpty = (p.equippedData[i].id == -1);
            Rectangle slotRect = { 120, (float)y, 260, 95 }; Rectangle btnRect = { 300, (float)y + 25, 70, 40 };
            Color slotCol = (p.activeSlot == i) ? MAROON : BLACK; if (showDetail) slotCol = ColorBrightness(slotCol, -0.4f);
            DrawRectangleRec(slotRect, slotCol);
            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) { if (IsMouseButtonPressed(0)) OpenDetail(p.equippedData[i]); }
            }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedData[i]).c_str(), { 130, (float)y + 25 }, 20, 1, Player::GetItemRarityColor(p.equippedData[i]));
                float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[i]);
                DrawTextEx(font, TextFormat("%s +%.1f", T("ATK", "ATK").c_str(), totalBonus), { 130, (float)y + 50 }, 14, 1, YELLOW);
                if (UI::DrawButton(btnRect, T("OFF", "OFF").c_str(), font, RED)) p.UnequipWeapon(i);
            }
            else DrawTextEx(font, T("EMPTY", "EMPTY").c_str(), { 130, (float)y + 35 }, 20, 1, DARKGRAY);
        }

        const char* armorKeys[] = { "HEAD", "CHEST", "HANDS", "LEGS", "FEET" };
        const char* armorNames[] = { "Head", "Chest", "Hands", "Legs", "Feet" };
        for (int i = 0; i < 5; i++) {
            int y = 160 + i * 70; DrawTextEx(font, T(armorKeys[i], armorNames[i]).c_str(), { 420, (float)y + 20 }, 16, 1, LIGHTGRAY);
            bool isEmpty = (p.equippedArmor[i].id == -1);
            Rectangle slotRect = { 480, (float)y, 200, 60 }; Rectangle btnRect = { 610, (float)y + 10, 70, 40 };
            DrawRectangleRec(slotRect, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK); DrawRectangleLinesEx(slotRect, 1, showDetail ? GRAY : DARKGRAY);
            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) { if (IsMouseButtonPressed(0)) OpenDetail(p.equippedArmor[i]); }
            }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedArmor[i]).c_str(), { 490, (float)y + 10 }, 14, 1, Player::GetItemRarityColor(p.equippedArmor[i]));
                float def = p.equippedArmor[i].defBonus + DataManager::GetModifier(p.equippedArmor[i].modifierId).def;
                DrawTextEx(font, TextFormat("%s +%.1f", T("DEF", "DEF").c_str(), def), { 490, (float)y + 35 }, 12, 1, BLUE);
                if (UI::DrawButton(btnRect, T("OFF", "OUT").c_str(), font, RED)) p.UnequipArmor(i);
            }
            else { DrawTextEx(font, T("EMPTY", "EMPTY").c_str(), { 490, (float)y + 20 }, 14, 1, DARKGRAY); }
        }

        DrawTextEx(font, T("OWNED_EQUIP", "Owned Equipment").c_str(), { 720, 130 }, 18, 1, GOLD);
        const int perP = 8; int maxP = (int)ceil((float)p.inventoryEquip.size() / perP); if (maxP < 1) maxP = 1;
        for (int i = 0; i < perP; i++) {
            int idx = equipPage * perP + i; if (idx >= (int)p.inventoryEquip.size()) break;
            int y = 160 + i * 45; Rectangle r = { 720, (float)y, 300, 40 };
            DrawRectangleRec(r, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r)) { if (GetMouseX() < 950) { if (IsMouseButtonPressed(0)) OpenDetail(p.inventoryEquip[idx]); } }
            DrawTextEx(font, Player::GetFullItemName(p.inventoryEquip[idx]).c_str(), { 730, (float)y + 10 }, 14, 1, Player::GetItemRarityColor(p.inventoryEquip[idx]));

            if (p.inventoryEquip[idx].type == "EQUIP") {
                // ★修正: 装備時に配列が変動するためbreak
                if (UI::DrawButton({ 940, (float)y, 40, 40 }, T("W1", "W1").c_str(), font, DARKGRAY)) { p.EquipWeapon(idx, 0); break; }
                if (UI::DrawButton({ 985, (float)y, 40, 40 }, T("W2", "W2").c_str(), font, DARKGRAY)) { p.EquipWeapon(idx, 1); break; }
            }
            else if (p.inventoryEquip[idx].type == "ARMOR") {
                int subtype = p.inventoryEquip[idx].weaponSubtype;
                if (subtype >= 0 && subtype < 5) {
                    if (UI::DrawButton({ 940, (float)y, 85, 40 }, T("EQUIP", "EQUIP").c_str(), font, DARKGREEN)) { p.EquipArmor(idx, subtype); break; }
                }
            }
        }
        if (UI::DrawButton({ 720, 530, 80, 30 }, "<<", font, GRAY) && equipPage > 0) equipPage--;
        if (UI::DrawButton({ 810, 530, 80, 30 }, ">>", font, GRAY) && equipPage < maxP - 1) equipPage++;
    }
    else if (tab == SKILL) {
        Rectangle viewArea = { 100, 120, (float)sw - 200, (float)sh - 170 };
        DrawTextEx(font, T("CAM_CONTROL", "Right Click & Drag to Move").c_str(), { 120, 620 }, 16, 1, LIGHTGRAY);
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !showDetail) { Vector2 delta = GetMouseDelta(); skillOffset = Vector2Add(skillOffset, delta); }
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);

        for (auto& node : p.skillTree) {
            Vector2 startPos = Vector2Add(node.uiPos, skillOffset);
            for (int reqId : node.reqIds) {
                int targetIdx = -1;
                for (int j = 0; j < (int)p.skillTree.size(); j++) {
                    if (p.skillTree[j].id == reqId) { targetIdx = j; break; }
                }
                if (targetIdx != -1) {
                    Vector2 endPos = Vector2Add(p.skillTree[targetIdx].uiPos, skillOffset);
                    DrawLineEx(startPos, endPos, 3, node.unlocked ? GOLD : DARKGRAY);
                }
            }
        }

        int hoveredSkillId = -1;

        for (int i = 0; i < (int)p.skillTree.size(); i++) {
            auto& node = p.skillTree[i];
            bool available = p.IsSkillAvailable(node.id);
            Vector2 drawPos = Vector2Add(node.uiPos, skillOffset);
            Color nodeColor = node.unlocked ? YELLOW : (available ? GREEN : DARKGRAY);

            if (node.type != SKILL_PASSIVE) {
                nodeColor = node.unlocked ? ORANGE : (available ? PURPLE : DARKGRAY);
            }

            DrawPoly(drawPos, 6, 35, 0, nodeColor);
            DrawPolyLines(drawPos, 6, 35, 0, RAYWHITE);
            DrawTextEx(font, node.name.c_str(), { drawPos.x - 28, drawPos.y - 8 }, 12, 1, node.unlocked ? BLACK : WHITE);

            if (!node.unlocked) {
                DrawTextEx(font, TextFormat("SP:%d", node.cost), { drawPos.x - 15, drawPos.y + 15 }, 10, 1, WHITE);
            }

            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea)) {
                if (CheckCollisionPointCircle(GetMousePosition(), drawPos, 35)) {
                    hoveredSkillId = i;
                    if (available && IsMouseButtonPressed(0)) p.UnlockSkill(node.id);
                }
            }
        }
        EndScissorMode();

        if (hoveredSkillId != -1) {
            auto& node = p.skillTree[hoveredSkillId];
            Vector2 mPos = GetMousePosition();
            DrawRectangle(mPos.x + 15, mPos.y + 15, 250, 100, Fade(BLACK, 0.9f));
            DrawRectangleLines(mPos.x + 15, mPos.y + 15, 250, 100, GOLD);
            DrawTextEx(font, node.name.c_str(), { mPos.x + 25, mPos.y + 25 }, 18, 1, WHITE);
            DrawTextEx(font, TextFormat(T("SKILL_COST", "Cost: %d SP").c_str(), node.cost), { mPos.x + 25, mPos.y + 45 }, 14, 1, YELLOW);
            DrawTextEx(font, node.desc.c_str(), { mPos.x + 25, mPos.y + 65 }, 12, 1, LIGHTGRAY);

            if (node.unlocked) {
                DrawTextEx(font, T("SKILL_UNLOCKED", "UNLOCKED").c_str(), { mPos.x + 160, mPos.y + 25 }, 14, 1, GREEN);
            }
            else if (p.IsSkillAvailable(node.id)) {
                DrawTextEx(font, T("SKILL_AVAILABLE", "AVAILABLE").c_str(), { mPos.x + 150, mPos.y + 25 }, 14, 1, SKYBLUE);
            }
            else {
                DrawTextEx(font, T("SKILL_LOCKED", "LOCKED").c_str(), { mPos.x + 170, mPos.y + 25 }, 14, 1, RED);
            }
        }
    }
    else if (tab == INVENTORY) {
        const char* subK[] = { "CONSUMABLE", "MATERIAL" };
        for (int i = 0; i < 2; i++) {
            Rectangle r = { 120.0f + (float)i * 210, 120, 200, 35 }; Color c = (itemSubTab == i) ? GREEN : BLACK; if (showDetail) c = ColorBrightness(c, -0.4f);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { itemSubTab = i; itemPage = 0; }
            DrawRectangleRec(r, c); std::string label = T(subK[i], subK[i]); DrawTextEx(font, label.c_str(), { r.x + 10, r.y + 8 }, 16, 1, WHITE);
        }
        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL"; for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perP = 10; int maxP = (int)ceil((float)filtered.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(T("PAGE_INFO", "Page %d/%d").c_str(), itemPage + 1, maxP), { 600, 125 }, 18, 1, WHITE);
        for (int i = 0; i < perP; i++) {
            int lIdx = itemPage * perP + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 165 + i * 42; Rectangle itemRect = { 120, (float)y, 400, 38 };
            DrawRectangleRec(itemRect, Fade(BLACK, showDetail ? 0.2f : 0.4f));
            DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), { 135, (float)y + 10 }, 18, 1.0f, Player::GetItemRarityColor(item));
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), itemRect)) { if (IsMouseButtonPressed(0)) OpenDetail(item); }

            // ★修正: 消費アイテム使用時に配列が変動するためbreak
            if (itemSubTab == 0 && UI::DrawButton({ 530, (float)y, 80, 38 }, T("USE", "Use").c_str(), font, GREEN)) {
                p.UseItem(invIdx);
                break;
            }
        }
        if (UI::DrawButton({ 120, 600, 100, 30 }, "<<", font, GRAY) && itemPage > 0) itemPage--;
        if (UI::DrawButton({ 230, 600, 100, 30 }, ">>", font, GRAY) && itemPage < maxP - 1) itemPage++;
    }
    else if (tab == DEBUG_TAB) {
        const int perP = 10; int total = (int)DataManager::itemConfigs.size(); int maxP = (int)ceil((float)total / perP);
        for (int i = 0; i < perP; i++) {
            int idx = debugPage * perP + i; if (idx >= total) break; auto& cfg = DataManager::itemConfigs[idx]; int y = 160 + i * 42; DrawRectangle(120, y, 450, 38, Fade(BLACK, 0.5f));
            DrawTextEx(font, TextFormat("[%s] %s", cfg.type.c_str(), cfg.name.c_str()), { 130, (float)y + 10 }, 16, 1, Player::GetItemRarityColor(cfg));
            if (UI::DrawButton({ 580, (float)y, 60, 38 }, T("GET", "Get").c_str(), font, RED)) p.AddToInventory(cfg);
        }
        if (UI::DrawButton({ 120, 600, 80, 30 }, "<<", font, GRAY) && debugPage > 0) debugPage--; if (UI::DrawButton({ 210, 600, 80, 30 }, ">>", font, GRAY) && debugPage < maxP - 1) debugPage++; if (UI::DrawButton({ 680, 160, 180, 50 }, "SP +999", font, BLUE)) p.skillPoints += 999;
    }
    else if (tab == MAP_TAB) {
        Rectangle viewArea = { 110, 120, (float)sw - 220, (float)sh - 170 };
        DrawTextEx(font, T("MAP_CONTROL", "Right Click & Drag to Move").c_str(), { 120, 620 }, 16, 1, LIGHTGRAY);
        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) { Vector2 delta = GetMouseDelta(); mapOffset = Vector2Add(mapOffset, delta); }
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
        float sc = 12.0f; float offX = (viewArea.x + viewArea.width / 2.0f) - (d.currentWidth * sc / 2.0f) + mapOffset.x; float offY = (viewArea.y + viewArea.height / 2.0f) - (d.currentHeight * sc / 2.0f) + mapOffset.y;
        DrawRectangle(offX - 5, offY - 5, d.currentWidth * sc + 10, d.currentHeight * sc + 10, BLACK);
        for (int y = 0; y < d.currentHeight; y++) {
            for (int x = 0; x < d.currentWidth; x++) {
                if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) {
                    DrawRectangle(offX + x * sc, offY + y * sc, sc - 1, sc - 1, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
                }
            }
        }
        DrawCircle(offX + (p.position.x / TILE_SIZE) * sc, offY + (p.position.z / TILE_SIZE) * sc, 5, RED); EndScissorMode();
    }
    else if (tab == SYSTEM_TAB) {
        DrawTextEx(font, T("SYS_MENU", "System Menu").c_str(), { 120, 130 }, 24, 1, WHITE);
        Rectangle saveBtn = { 120, 200, 200, 60 };
        if (!d.isHome) { DrawRectangleRec(saveBtn, GRAY); DrawTextEx(font, T("SAVE_HOME_ONLY", "Save (Home Only)").c_str(), { 130, 220 }, 18, 1, DARKGRAY); }
        else { if (UI::DrawButton(saveBtn, T("SAVE_GAME", "Save Game").c_str(), font, BLUE)) { eventCode = 1; } DrawTextEx(font, T("SAVE_DESC", "Save your progress").c_str(), { 340, 220 }, 18, 1, LIGHTGRAY); }
        Rectangle titleBtn = { 120, 300, 200, 60 }; if (UI::DrawButton(titleBtn, T("RETURN_TITLE", "Return to Title").c_str(), font, RED)) { eventCode = 2; }
    }
    else if (tab == OPTION_TAB) {
        DrawTextEx(font, T("SOUND_SETTING", "Sound Settings").c_str(), { 150, 150 }, 24, 1, WHITE);
        DrawTextEx(font, TextFormat(T("VOL_BGM", "BGM Volume: %.1f").c_str(), AudioManager::bgmVolume), { 150, 200 }, 20, 1, WHITE);
        Rectangle bgmBar = { 150, 230, 300, 20 }; DrawRectangleRec(bgmBar, GRAY); DrawRectangle(bgmBar.x, bgmBar.y, bgmBar.width * AudioManager::bgmVolume, bgmBar.height, GREEN);
        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 220, 300, 40 }) && IsMouseButtonDown(0)) { float newVol = (GetMouseX() - 150) / 300.0f; AudioManager::SetBGMVolume(newVol); }

        DrawTextEx(font, TextFormat(T("VOL_SE", "SE Volume: %.1f").c_str(), AudioManager::seVolume), { 150, 300 }, 20, 1, WHITE);
        Rectangle seBar = { 150, 330, 300, 20 }; DrawRectangleRec(seBar, GRAY); DrawRectangle(seBar.x, seBar.y, seBar.width * AudioManager::seVolume, seBar.height, ORANGE);
        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 320, 300, 40 }) && IsMouseButtonDown(0)) { float newVol = (GetMouseX() - 150) / 300.0f; AudioManager::SetSEVolume(newVol); if (IsMouseButtonPressed(0)) AudioManager::PlaySE(SE_CLICK); }
    }

    DrawDetailWindow(font);
    return eventCode;
}