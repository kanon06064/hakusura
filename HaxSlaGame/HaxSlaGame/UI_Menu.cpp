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

    DrawRectangle(50, 30, sw - 100, sh - 60, Fade(DARKGRAY, 0.95f));
    DrawRectangleLinesEx({ 50, 30, (float)sw - 100, (float)sh - 60 }, 2, GRAY);

    const char* tKeys[] = { "EQUIP", "SKILL", "MAP", "ITEMS", "SYSTEM", "OPTION", "CONTROL" };
    const char* tDefs[] = { "Equip", "Skill", "Map", "Items", "System", "Option", "Control" };

    for (int i = 0; i < 7; i++) {
        Rectangle r = { 65.0f + (float)i * 155, 50.0f, 142.0f, 42.0f };
        Color tabColor = (tab == i) ? BLUE : DARKGRAY;
        if (UI::DrawButton(r, T(tKeys[i], tDefs[i]).c_str(), font, tabColor)) { tab = (MenuTab)i; }
    }

    bool clickInput = IsMouseButtonPressed(0) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool downInput = IsMouseButtonDown(0) || IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool rightDownInput = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);

    if (tab == EQUIP) {
        float leftX = 75.0f;
        float labelX = 420.0f;
        float midX = 465.0f;
        float rightX = 805.0f;

        DrawTextEx(font, T("ACTIVE_SLOTS", "Equipped Weapons").c_str(), { leftX, 115 }, 22, 1, GOLD);
        for (int i = 0; i < 2; i++) {
            int y = 150 + i * 115;
            bool isEmpty = (p.equippedData[i].id == -1);

            Rectangle slotRect = { leftX, (float)y, 320, 100 };
            Rectangle btnRect = { leftX + 235, (float)y + 28, 72, 44 };
            Color slotCol = (p.activeSlot == i) ? MAROON : BLACK;
            if (showDetail) slotCol = ColorBrightness(slotCol, -0.4f);

            UI::RegisterInteractable(slotRect);
            DrawRectangleRec(slotRect, slotCol);
            DrawRectangleLinesEx(slotRect, 2, (p.activeSlot == i) ? GOLD : DARKGRAY);

            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) {
                    if (clickInput) OpenDetail(p.equippedData[i]);
                }
            }

            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedData[i]).c_str(), { leftX + 12, (float)y + 20 }, 20, 1, Player::GetItemRarityColor(p.equippedData[i]));
                float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[i]);
                DrawTextEx(font, TextFormat("%s +%.1f", T("ATK", "ATK").c_str(), totalBonus), { leftX + 12, (float)y + 55 }, 16, 1, YELLOW);

                if (UI::DrawButton(btnRect, T("OFF", "OFF").c_str(), font, RED)) p.UnequipWeapon(i);
            }
            else {
                DrawTextEx(font, T("EMPTY", "EMPTY").c_str(), { leftX + 15, (float)y + 38 }, 20, 1, DARKGRAY);
            }
        }

        const char* armorKeys[] = { "HEAD", "CHEST", "HANDS", "LEGS", "FEET" };
        const char* armorNames[] = { "Head", "Chest", "Hands", "Legs", "Feet" };

        for (int i = 0; i < 5; i++) {
            int y = 150 + i * 74;

            DrawTextEx(font, T(armorKeys[i], armorNames[i]).c_str(), { labelX, (float)y + 22 }, 18, 1, LIGHTGRAY);

            bool isEmpty = (p.equippedArmor[i].id == -1);
            Rectangle slotRect = { midX, (float)y, 315, 64 };
            Rectangle btnRect = { midX + 238, (float)y + 11, 65, 42 };

            UI::RegisterInteractable(slotRect);
            DrawRectangleRec(slotRect, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK);
            DrawRectangleLinesEx(slotRect, 1, showDetail ? GRAY : DARKGRAY);

            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) {
                    if (clickInput) OpenDetail(p.equippedArmor[i]);
                }
            }

            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedArmor[i]).c_str(), { midX + 10, (float)y + 12 }, 18, 1, Player::GetItemRarityColor(p.equippedArmor[i]));
                float def = p.equippedArmor[i].defBonus + DataManager::GetModifier(p.equippedArmor[i].modifierId).def;
                DrawTextEx(font, TextFormat("%s +%.1f", T("DEF", "DEF").c_str(), def), { midX + 10, (float)y + 36 }, 14, 1, BLUE);

                if (UI::DrawButton(btnRect, T("OFF", "OUT").c_str(), font, RED)) p.UnequipArmor(i);
            }
            else {
                DrawTextEx(font, T("EMPTY", "EMPTY").c_str(), { midX + 12, (float)y + 22 }, 18, 1, DARKGRAY);
            }
        }

        DrawTextEx(font, T("OWNED_EQUIP", "Owned Equipment").c_str(), { rightX, 115 }, 22, 1, GOLD);
        const int perP = 8;
        int maxP = (int)ceil((float)p.inventoryEquip.size() / perP);
        if (maxP < 1) maxP = 1;

        for (int i = 0; i < perP; i++) {
            int idx = equipPage * perP + i;
            if (idx >= (int)p.inventoryEquip.size()) break;

            int y = 150 + i * 48;
            Rectangle r = { rightX, (float)y, 385, 44 };

            UI::RegisterInteractable(r);
            DrawRectangleRec(r, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK);
            DrawRectangleLinesEx(r, 1, DARKGRAY);

            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r)) {
                if (GetMouseX() < rightX + 280) {
                    if (clickInput) OpenDetail(p.inventoryEquip[idx]);
                }
            }

            int comp = Player::CompareWithEquipped(p.inventoryEquip[idx], p);
            float cx = rightX + 12.0f;
            float cy = (float)y + 22.0f;

            if (comp == 1) {
                Vector2 pTop = { cx, cy - 6.0f };
                Vector2 pRight = { cx + 6.0f, cy + 6.0f };
                Vector2 pLeft = { cx - 6.0f, cy + 6.0f };
                DrawTriangle(pTop, pRight, pLeft, LIME);
                DrawTriangle(pTop, pLeft, pRight, LIME);
            }
            else if (comp == -1) {
                Vector2 pBottom = { cx, cy + 6.0f };
                Vector2 pLeft = { cx - 6.0f, cy - 6.0f };
                Vector2 pRight = { cx + 6.0f, cy - 6.0f };
                DrawTriangle(pBottom, pLeft, pRight, RED);
                DrawTriangle(pBottom, pRight, pLeft, RED);
            }

            DrawTextEx(font, Player::GetFullItemName(p.inventoryEquip[idx]).c_str(), { rightX + 26.0f, (float)y + 12 }, 18, 1, Player::GetItemRarityColor(p.inventoryEquip[idx]));

            if (p.inventoryEquip[idx].type == "EQUIP") {
                if (UI::DrawButton({ rightX + 292, (float)y + 4, 42, 36 }, T("W1", "W1").c_str(), font, DARKGRAY)) { p.EquipWeapon(idx, 0); break; }
                if (UI::DrawButton({ rightX + 338, (float)y + 4, 42, 36 }, T("W2", "W2").c_str(), font, DARKGRAY)) { p.EquipWeapon(idx, 1); break; }
            }
            else if (p.inventoryEquip[idx].type == "ARMOR") {
                int subtype = p.inventoryEquip[idx].weaponSubtype;
                if (subtype >= 0 && subtype < 5) {
                    if (UI::DrawButton({ rightX + 292, (float)y + 4, 88, 36 }, T("EQUIP", "EQUIP").c_str(), font, DARKGREEN)) { p.EquipArmor(idx, subtype); break; }
                }
            }
        }

        float pageBtnY = 550.0f;
        if (UI::DrawButton({ rightX + 40, pageBtnY, 80, 35 }, "<<", font, GRAY) && equipPage > 0) equipPage--;
        if (UI::DrawButton({ rightX + 240, pageBtnY, 80, 35 }, ">>", font, GRAY) && equipPage < maxP - 1) equipPage++;
    }
    // =========================================================================
    // ★ SKILL タブ（全43ノード対応 ＆ キーストーン大ノード描画）
    // =========================================================================
    else if (tab == SKILL) {
        Rectangle viewArea = { 100, 120, (float)sw - 200, (float)sh - 170 };
        DrawTextEx(font, T("CAM_CONTROL", "Right Click & Drag to Move").c_str(), { 120, (float)sh - 85 }, 16, 1, LIGHTGRAY);
        DrawTextEx(font, TextFormat(T("HUD_GOLD_SP", "SP: %d").c_str(), p.skillPoints), { (float)sw - 220, 130 }, 20, 1, YELLOW);

        if (rightDownInput && !showDetail) {
            Vector2 delta = GetMouseDelta();
            if (IsGamepadAvailable(0)) {
                delta.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 10.0f;
                delta.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 10.0f;
            }
            skillOffset = Vector2Add(skillOffset, delta);
        }

        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);

        // 1. ノード間の接続ライン（前提条件線）描画
        for (const auto& node : p.skillTree) {
            Vector2 startPos = Vector2Add(node.uiPos, skillOffset);
            for (int reqId : node.reqIds) {
                int targetIdx = -1;
                for (int j = 0; j < (int)p.skillTree.size(); j++) {
                    if (p.skillTree[j].id == reqId) { targetIdx = j; break; }
                }
                if (targetIdx != -1) {
                    Vector2 endPos = Vector2Add(p.skillTree[targetIdx].uiPos, skillOffset);
                    Color lineCol = node.unlocked ? GOLD : DARKGRAY;
                    float lineThick = (node.keystoneType > 0) ? 4.0f : 2.5f;
                    DrawLineEx(startPos, endPos, lineThick, lineCol);
                }
            }
        }

        int hoveredSkillIdx = -1;

        // 2. 全43ノードの本体描画
        for (int i = 0; i < (int)p.skillTree.size(); i++) {
            auto& node = p.skillTree[i];
            bool available = p.IsSkillAvailable(node.id);
            Vector2 drawPos = Vector2Add(node.uiPos, skillOffset);

            // ノード半径: キーストーンは大ノード(46px)、一般は35px
            float nodeRadius = (node.keystoneType > 0) ? 46.0f : 34.0f;

            Color nodeColor = node.unlocked ? YELLOW : (available ? GREEN : DARKGRAY);
            if (node.type != SKILL_PASSIVE) {
                nodeColor = node.unlocked ? ORANGE : (available ? PURPLE : DARKGRAY);
            }
            if (node.keystoneType > 0) {
                nodeColor = node.unlocked ? GOLD : (available ? MAROON : Color{ 40, 20, 20, 255 });
            }

            Rectangle nodeRect = { drawPos.x - nodeRadius, drawPos.y - nodeRadius, nodeRadius * 2, nodeRadius * 2 };
            UI::RegisterInteractable(nodeRect);

            // ★ キーストーンは大ノード（八角形＋二重枠）で圧倒的差別化
            if (node.keystoneType > 0) {
                DrawPoly(drawPos, 8, nodeRadius, 0, nodeColor);
                DrawPolyLines(drawPos, 8, nodeRadius, 0, node.unlocked ? GOLD : RAYWHITE);
                DrawPolyLines(drawPos, 8, nodeRadius + 3.0f, 0, node.unlocked ? WHITE : Fade(GRAY, 0.6f));
            }
            else {
                DrawPoly(drawPos, 6, nodeRadius, 0, nodeColor);
                DrawPolyLines(drawPos, 6, nodeRadius, 0, RAYWHITE);
            }

            // ノード名称
            Vector2 tSize = MeasureTextEx(font, node.name.c_str(), (node.keystoneType > 0) ? 13 : 11, 1);
            DrawTextEx(font, node.name.c_str(), { drawPos.x - tSize.x / 2.0f, drawPos.y - tSize.y / 2.0f }, (node.keystoneType > 0) ? 13 : 11, 1, node.unlocked ? BLACK : WHITE);

            if (!node.unlocked) {
                std::string spTxt = TextFormat("SP:%d", node.cost);
                Vector2 spSize = MeasureTextEx(font, spTxt.c_str(), 10, 1);
                DrawTextEx(font, spTxt.c_str(), { drawPos.x - spSize.x / 2.0f, drawPos.y + nodeRadius * 0.45f }, 10, 1, WHITE);
            }

            // クリック判定
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea)) {
                if (CheckCollisionPointCircle(GetMousePosition(), drawPos, nodeRadius)) {
                    hoveredSkillIdx = i;
                    if (available && clickInput) p.UnlockSkill(node.id);
                }
            }
        }
        EndScissorMode();

        // 3. マウスホバー時のスキル詳細ポップアップ
        if (hoveredSkillIdx != -1) {
            auto& node = p.skillTree[hoveredSkillIdx];
            Vector2 mPos = GetMousePosition();

            int popW = (node.keystoneType > 0) ? 360 : 310;
            int popH = (node.keystoneType > 0) ? 130 : 105;
            float px = mPos.x + 15; float py = mPos.y + 15;
            if (px + popW > sw - 60) px = mPos.x - popW - 10;
            if (py + popH > sh - 40) py = mPos.y - popH - 10;

            DrawRectangle(px, py, popW, popH, Fade(BLACK, 0.92f));
            DrawRectangleLinesEx({ px, py, (float)popW, (float)popH }, (node.keystoneType > 0) ? 3 : 2, (node.keystoneType > 0) ? GOLD : SKYBLUE);

            std::string headerName = node.name;
            if (node.keystoneType > 0) headerName = "【KEYSTONE】" + headerName;
            DrawTextEx(font, headerName.c_str(), { px + 12, py + 10 }, 20, 1, (node.keystoneType > 0) ? GOLD : WHITE);

            DrawTextEx(font, TextFormat(T("SKILL_COST", "Cost: %d SP").c_str(), node.cost), { px + 12, py + 34 }, 14, 1, YELLOW);
            DrawTextEx(font, node.desc.c_str(), { px + 12, py + 56 }, 14, 1, LIGHTGRAY);

            if (node.unlocked) DrawTextEx(font, T("SKILL_UNLOCKED", "UNLOCKED").c_str(), { px + popW - 95, py + 12 }, 15, 1, GREEN);
            else if (p.IsSkillAvailable(node.id)) DrawTextEx(font, T("SKILL_AVAILABLE", "AVAILABLE").c_str(), { px + popW - 105, py + 12 }, 15, 1, SKYBLUE);
            else DrawTextEx(font, T("SKILL_LOCKED", "LOCKED").c_str(), { px + popW - 85, py + 12 }, 15, 1, RED);
        }
    }
    else if (tab == INVENTORY) {
        const char* subK[] = { "CONSUMABLE", "MATERIAL" };
        for (int i = 0; i < 2; i++) {
            Rectangle r = { 120.0f + (float)i * 210, 120, 200, 35 }; Color c = (itemSubTab == i) ? GREEN : BLACK; if (showDetail) c = ColorBrightness(c, -0.4f);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r) && clickInput) { itemSubTab = i; itemPage = 0; }
            DrawRectangleRec(r, c); std::string label = T(subK[i], subK[i]); DrawTextEx(font, label.c_str(), { r.x + 10, r.y + 8 }, 16, 1, WHITE);
            UI::RegisterInteractable(r);
        }

        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL"; for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perP = 10; int maxP = (int)ceil((float)filtered.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(T("PAGE_INFO", "Page %d/%d").c_str(), itemPage + 1, maxP), { (float)sw / 2.0f, 125 }, 18, 1, WHITE);

        float listW = (float)sw / 2.0f - 100.0f;
        for (int i = 0; i < perP; i++) {
            int lIdx = itemPage * perP + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 165 + i * 42; Rectangle itemRect = { 120, (float)y, listW, 38 };

            UI::RegisterInteractable(itemRect);
            DrawRectangleRec(itemRect, Fade(BLACK, showDetail ? 0.2f : 0.4f));
            DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), { 135, (float)y + 10 }, 18, 1.0f, Player::GetItemRarityColor(item));
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), itemRect)) { if (clickInput) OpenDetail(item); }

            if (itemSubTab == 0 && UI::DrawButton({ 120 + listW + 10, (float)y, 80, 38 }, T("USE", "Use").c_str(), font, GREEN)) {
                p.UseItem(invIdx);
                break;
            }
        }
        if (UI::DrawButton({ 120, (float)sh - 120, 100, 30 }, "<<", font, GRAY) && itemPage > 0) itemPage--;
        if (UI::DrawButton({ 230, (float)sh - 120, 100, 30 }, ">>", font, GRAY) && itemPage < maxP - 1) itemPage++;
    }
    else if (tab == MAP_TAB) {
        Rectangle viewArea = { 110, 120, (float)sw - 220, (float)sh - 170 };
        DrawTextEx(font, T("MAP_CONTROL", "Right Click & Drag to Move").c_str(), { 120, (float)sh - 100 }, 16, 1, LIGHTGRAY);
        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea) && rightDownInput) {
            Vector2 delta = GetMouseDelta();
            if (IsGamepadAvailable(0)) {
                delta.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 10.0f;
                delta.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 10.0f;
            }
            mapOffset = Vector2Add(mapOffset, delta);
        }
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
        float sc = 12.0f;
        float offX = (viewArea.x + viewArea.width / 2.0f) - (d.currentWidth * sc / 2.0f) + mapOffset.x; float offY = (viewArea.y + viewArea.height / 2.0f) - (d.currentHeight * sc / 2.0f) + mapOffset.y;
        DrawRectangle(offX - 5, offY - 5, d.currentWidth * sc + 10, d.currentHeight * sc + 10, BLACK);

        for (int y = 0; y < d.currentHeight; y++) {
            for (int x = 0; x < d.currentWidth; x++) {
                if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) {
                    DrawRectangle(offX + x * sc, offY + y * sc, sc - 1, sc - 1, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
                }
            }
        }
        DrawCircle(offX + (p.position.x / TILE_SIZE) * sc, offY + (p.position.z / TILE_SIZE) * sc, 5, RED);
        EndScissorMode();
    }
    else if (tab == SYSTEM_TAB) {
        float centerX = sw / 2.0f;
        DrawTextEx(font, T("SYS_MENU", "System Menu").c_str(), { centerX - 100, 130 }, 24, 1, WHITE);

        Rectangle saveBtn = { centerX - 100, 200, 200, 60 };
        if (!d.isHome) { DrawRectangleRec(saveBtn, GRAY); DrawTextEx(font, T("SAVE_HOME_ONLY", "Save (Home Only)").c_str(), { centerX - 90, 220 }, 18, 1, DARKGRAY); }
        else { if (UI::DrawButton(saveBtn, T("SAVE_GAME", "Save Game").c_str(), font, BLUE)) { eventCode = 1; } DrawTextEx(font, T("SAVE_DESC", "Save your progress").c_str(), { centerX + 120, 220 }, 18, 1, LIGHTGRAY); }

        Rectangle titleBtn = { centerX - 100, 300, 200, 60 }; if (UI::DrawButton(titleBtn, T("RETURN_TITLE", "Return to Title").c_str(), font, RED)) { eventCode = 2; }
    }
    else if (tab == OPTION_TAB) {
        float rightColX = (float)sw - 630.0f;

        DrawTextEx(font, T("SOUND_SETTING", "Sound Settings").c_str(), { 150, 130 }, 24, 1, WHITE);

        int bgmVolInt = (int)roundf(AudioManager::bgmVolume * 100.0f);
        DrawTextEx(font, TextFormat(T("VOL_BGM", "BGM Volume: %d").c_str(), bgmVolInt), { 150, 180 }, 20, 1, WHITE);

        Rectangle bgmBar = { 150, 210, 300, 20 };
        UI::RegisterInteractable(bgmBar);
        DrawRectangleRec(bgmBar, GRAY);
        DrawRectangle(bgmBar.x, bgmBar.y, bgmBar.width * AudioManager::bgmVolume, bgmBar.height, GREEN);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 200, 300, 40 }) && downInput) {
            float newVol = (GetMouseX() - 150) / 300.0f;
            AudioManager::SetBGMVolume(newVol); DataManager::SaveConfig();
        }

        if (UI::DrawButton({ 470, 200, 40, 40 }, "-", font, GRAY)) { AudioManager::SetBGMVolume(AudioManager::bgmVolume - 0.05f); DataManager::SaveConfig(); }
        if (UI::DrawButton({ 520, 200, 40, 40 }, "+", font, GRAY)) { AudioManager::SetBGMVolume(AudioManager::bgmVolume + 0.05f); DataManager::SaveConfig(); }

        int seVolInt = (int)roundf(AudioManager::seVolume * 100.0f);
        DrawTextEx(font, TextFormat(T("VOL_SE", "SE Volume: %d").c_str(), seVolInt), { 150, 270 }, 20, 1, WHITE);

        Rectangle seBar = { 150, 300, 300, 20 };
        UI::RegisterInteractable(seBar);
        DrawRectangleRec(seBar, GRAY);
        DrawRectangle(seBar.x, seBar.y, seBar.width * AudioManager::seVolume, seBar.height, ORANGE);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 290, 300, 40 }) && downInput) {
            float newVol = (GetMouseX() - 150) / 300.0f;
            AudioManager::SetSEVolume(newVol); DataManager::SaveConfig();
            if (clickInput) AudioManager::PlaySE(SE_CLICK);
        }

        if (UI::DrawButton({ 470, 290, 40, 40 }, "-", font, GRAY)) { AudioManager::SetSEVolume(AudioManager::seVolume - 0.05f); DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK); }
        if (UI::DrawButton({ 520, 290, 40, 40 }, "+", font, GRAY)) { AudioManager::SetSEVolume(AudioManager::seVolume + 0.05f); DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK); }

        DrawTextEx(font, T("CTRL_SCREEN_SETTING", "Control & Screen").c_str(), { rightColX, 130 }, 24, 1, WHITE);

        DrawTextEx(font, TextFormat(T("SENS_MOUSE", "Mouse Sensitivity: %.1f").c_str(), DataManager::keyConfig.mouseSensitivity), { rightColX, 180 }, 20, 1, WHITE);
        Rectangle mSensBar = { rightColX, 210, 300, 20 };
        UI::RegisterInteractable(mSensBar);
        DrawRectangleRec(mSensBar, GRAY);
        float mRatio = (DataManager::keyConfig.mouseSensitivity - 0.1f) / 4.9f; if (mRatio < 0) mRatio = 0; if (mRatio > 1) mRatio = 1;
        DrawRectangle(mSensBar.x, mSensBar.y, mSensBar.width * mRatio, mSensBar.height, SKYBLUE);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { rightColX, 200, 300, 40 }) && downInput) {
            float newRatio = (GetMouseX() - rightColX) / 300.0f; if (newRatio < 0) newRatio = 0; if (newRatio > 1) newRatio = 1;
            DataManager::keyConfig.mouseSensitivity = 0.1f + newRatio * 4.9f; DataManager::SaveConfig();
        }
        if (UI::DrawButton({ rightColX + 320, 200, 40, 40 }, "-", font, GRAY)) { DataManager::keyConfig.mouseSensitivity -= 0.1f; if (DataManager::keyConfig.mouseSensitivity < 0.1f) DataManager::keyConfig.mouseSensitivity = 0.1f; DataManager::SaveConfig(); }
        if (UI::DrawButton({ rightColX + 370, 200, 40, 40 }, "+", font, GRAY)) { DataManager::keyConfig.mouseSensitivity += 0.1f; if (DataManager::keyConfig.mouseSensitivity > 5.0f) DataManager::keyConfig.mouseSensitivity = 5.0f; DataManager::SaveConfig(); }

        DrawTextEx(font, TextFormat(T("SENS_PAD", "Pad Sensitivity: %.1f").c_str(), DataManager::keyConfig.padSensitivity), { rightColX, 270 }, 20, 1, WHITE);
        Rectangle pSensBar = { rightColX, 300, 300, 20 };
        UI::RegisterInteractable(pSensBar);
        DrawRectangleRec(pSensBar, GRAY);
        float pRatio = (DataManager::keyConfig.padSensitivity - 0.1f) / 4.9f; if (pRatio < 0) pRatio = 0; if (pRatio > 1) pRatio = 1;
        DrawRectangle(pSensBar.x, pSensBar.y, pSensBar.width * pRatio, pSensBar.height, PURPLE);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { rightColX, 290, 300, 40 }) && downInput) {
            float newRatio = (GetMouseX() - rightColX) / 300.0f; if (newRatio < 0) newRatio = 0; if (newRatio > 1) newRatio = 1;
            DataManager::keyConfig.padSensitivity = 0.1f + newRatio * 4.9f; DataManager::SaveConfig();
        }
        if (UI::DrawButton({ rightColX + 320, 290, 40, 40 }, "-", font, GRAY)) { DataManager::keyConfig.padSensitivity -= 0.1f; if (DataManager::keyConfig.padSensitivity < 0.1f) DataManager::keyConfig.padSensitivity = 0.1f; DataManager::SaveConfig(); }
        if (UI::DrawButton({ rightColX + 370, 290, 40, 40 }, "+", font, GRAY)) { DataManager::keyConfig.padSensitivity += 0.1f; if (DataManager::keyConfig.padSensitivity > 5.0f) DataManager::keyConfig.padSensitivity = 5.0f; DataManager::SaveConfig(); }

        DrawTextEx(font, T("SCREEN_MODE", "Screen Mode").c_str(), { rightColX, 370 }, 20, 1, WHITE);
        bool isFS = DataManager::keyConfig.isFullscreen;
        if (UI::DrawButton({ rightColX, 410, 150, 40 }, T("WINDOWED", "Windowed").c_str(), font, isFS ? DARKGRAY : BLUE)) {
            if (isFS) {
                ToggleFullscreen(); SetWindowSize(1280, 720);
                DataManager::keyConfig.isFullscreen = false; DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK);
            }
        }
        if (UI::DrawButton({ rightColX + 170, 410, 150, 40 }, T("FULLSCREEN", "Fullscreen").c_str(), font, isFS ? BLUE : DARKGRAY)) {
            if (!isFS) {
                int monitor = GetCurrentMonitor(); SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
                ToggleFullscreen();
                DataManager::keyConfig.isFullscreen = true; DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK);
            }
        }

        if (UI::DrawButton({ (float)sw - 350, (float)sh - 170, 200, 50 }, T("RESET_CONFIG", "Reset Settings").c_str(), font, MAROON)) {
            DataManager::ResetConfig();
            if (IsWindowFullscreen()) { ToggleFullscreen(); SetWindowSize(1280, 720); }
            AudioManager::PlaySE(SE_SAVE);
        }
    }
    else if (tab == CONTROL_TAB) {
        static int waitingForKeyIndex = -1;
        static bool waitingForPad = false;

        float centerX = sw / 2.0f;
        DrawTextEx(font, T("KEY_CONFIG", "Key Configuration").c_str(), { centerX - 250, 130 }, 24, 1, WHITE);

        struct BindInfo { const char* label; const char* tKey; int* keyPtr; int* padPtr; };
        BindInfo binds[] = {
            {"Move Forward", "KEY_FWD", &DataManager::keyConfig.moveForward, nullptr},
            {"Move Backward","KEY_BACK",&DataManager::keyConfig.moveBackward, nullptr},
            {"Move Left",    "KEY_LEFT",&DataManager::keyConfig.moveLeft, nullptr},
            {"Move Right",   "KEY_RIGHT",&DataManager::keyConfig.moveRight, nullptr},
            {"Attack",       "KEY_ATK", nullptr, &DataManager::keyConfig.padAttack},
            {"Dash",         "KEY_DASH",&DataManager::keyConfig.dash, &DataManager::keyConfig.padDash},
            {"Smash",        "KEY_SMASH",&DataManager::keyConfig.smash, &DataManager::keyConfig.padSmash},
            {"Kongo",        "KEY_KONGO",&DataManager::keyConfig.kongo, &DataManager::keyConfig.padKongo},
            {"Zoukyou",      "KEY_ZOUKYOU",&DataManager::keyConfig.zoukyou, &DataManager::keyConfig.padZoukyou},
            {"Stealth",      "KEY_STEALTH",&DataManager::keyConfig.stealth, &DataManager::keyConfig.padStealth},
            {"Heal",         "KEY_HEAL",&DataManager::keyConfig.heal, &DataManager::keyConfig.padHeal},
            {"Swap Weapon",  "KEY_SWAP",&DataManager::keyConfig.swapWeapon, &DataManager::keyConfig.padSwap},
        };

        if (waitingForKeyIndex != -1) {
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));
            DrawTextEx(font, T("PRESS_ANY_KEY", "Press any key to assign...").c_str(), { (float)sw / 2 - 200, (float)sh / 2 - 20 }, 24, 1, YELLOW);

            if (waitingForPad) {
                for (int i = 1; i < 32; i++) {
                    if (IsGamepadButtonPressed(0, i)) {
                        *binds[waitingForKeyIndex].padPtr = i; waitingForKeyIndex = -1; DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK); break;
                    }
                }
            }
            else {
                int keyPressed = GetKeyPressed();
                if (keyPressed > 0) {
                    *binds[waitingForKeyIndex].keyPtr = keyPressed; waitingForKeyIndex = -1; DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK);
                }
            }
        }
        else {
            for (int i = 0; i < 12; i++) {
                int col = i / 6; int row = i % 6;
                float x = centerX - 400.0f + col * 450.0f; float y = 180.0f + row * 60.0f;

                DrawTextEx(font, T(binds[i].tKey, binds[i].label).c_str(), { x, y + 10 }, 20, 1, LIGHTGRAY);

                if (binds[i].keyPtr != nullptr) {
                    std::string keyName = "KEY_" + std::to_string(*binds[i].keyPtr);
                    if (*binds[i].keyPtr >= 32 && *binds[i].keyPtr <= 126) keyName = std::string(1, (char)*binds[i].keyPtr);
                    else if (*binds[i].keyPtr == KEY_LEFT_SHIFT || *binds[i].keyPtr == KEY_RIGHT_SHIFT) keyName = "SHIFT";
                    else if (*binds[i].keyPtr == KEY_SPACE) keyName = "SPACE";

                    Rectangle btnR = { x + 160, y, 90, 40 };
                    if (UI::DrawButton(btnR, keyName.c_str(), font, DARKGRAY)) { waitingForKeyIndex = i; waitingForPad = false; AudioManager::PlaySE(SE_CLICK); }
                }
                else { DrawTextEx(font, T("LCLICK", "L-Click").c_str(), { x + 175, y + 10 }, 16, 1, GRAY); }

                if (binds[i].padPtr != nullptr) {
                    std::string padName = UI::GetPadBtnStr(*binds[i].padPtr);
                    Rectangle btnR = { x + 260, y, 90, 40 };
                    if (UI::DrawButton(btnR, padName.c_str(), font, Fade(DARKBLUE, 0.8f))) { waitingForKeyIndex = i; waitingForPad = true; AudioManager::PlaySE(SE_CLICK); }
                }
                else { DrawTextEx(font, T("LSTICK", "L-Stick").c_str(), { x + 275, y + 10 }, 16, 1, GRAY); }
            }

            if (UI::DrawButton({ (float)sw - 350, (float)sh - 170, 200, 50 }, T("RESET_CONFIG", "Reset Settings").c_str(), font, MAROON)) {
                DataManager::ResetConfig();
                if (IsWindowFullscreen()) { ToggleFullscreen(); SetWindowSize(1280, 720); }
                AudioManager::PlaySE(SE_SAVE);
            }
        }
    }

    DrawDetailWindow(font, p);
    return eventCode;
}