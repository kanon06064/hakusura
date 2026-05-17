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

    const char* tKeys[] = { "EQUIP", "SKILL", "MAP", "ITEMS", "DEBUG", "SYSTEM", "OPTION", "CONTROL" };
    const char* tDefs[] = { "Equip", "Skill", "Map", "Items", "Debug", "System", "Option", "Control" };

    for (int i = 0; i < 8; i++) {
        Rectangle r = { 100.0f + (float)i * 120, 70.0f, 115.0f, 40.0f };
        Color tabColor = (tab == i) ? BLUE : DARKGRAY;
        if (UI::DrawButton(r, T(tKeys[i], tDefs[i]).c_str(), font, tabColor)) { tab = (MenuTab)i; }
    }

    bool clickInput = IsMouseButtonPressed(0) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool downInput = IsMouseButtonDown(0) || IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool rightDownInput = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);

    if (tab == EQUIP) {
        DrawTextEx(font, T("ACTIVE_SLOTS", "Equipped").c_str(), { 120, 130 }, 20, 1, GOLD);
        for (int i = 0; i < 2; i++) {
            int y = 160 + i * 105; bool isEmpty = (p.equippedData[i].id == -1);
            Rectangle slotRect = { 120, (float)y, 260, 95 }; Rectangle btnRect = { 300, (float)y + 25, 70, 40 };
            Color slotCol = (p.activeSlot == i) ? MAROON : BLACK; if (showDetail) slotCol = ColorBrightness(slotCol, -0.4f);

            UI::RegisterInteractable(slotRect);

            DrawRectangleRec(slotRect, slotCol);
            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) { if (clickInput) OpenDetail(p.equippedData[i]); }
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

            UI::RegisterInteractable(slotRect);

            DrawRectangleRec(slotRect, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK); DrawRectangleLinesEx(slotRect, 1, showDetail ? GRAY : DARKGRAY);
            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) { if (clickInput) OpenDetail(p.equippedArmor[i]); }
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

            UI::RegisterInteractable(r);

            DrawRectangleRec(r, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r)) { if (GetMouseX() < 950) { if (clickInput) OpenDetail(p.inventoryEquip[idx]); } }
            DrawTextEx(font, Player::GetFullItemName(p.inventoryEquip[idx]).c_str(), { 730, (float)y + 10 }, 14, 1, Player::GetItemRarityColor(p.inventoryEquip[idx]));

            if (p.inventoryEquip[idx].type == "EQUIP") {
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
        if (rightDownInput && !showDetail) {
            Vector2 delta = GetMouseDelta();
            if (IsGamepadAvailable(0)) {
                delta.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 10.0f;
                delta.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 10.0f;
            }
            skillOffset = Vector2Add(skillOffset, delta);
        }
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

            Rectangle nodeRect = { drawPos.x - 35, drawPos.y - 35, 70, 70 };
            UI::RegisterInteractable(nodeRect);

            DrawPoly(drawPos, 6, 35, 0, nodeColor);
            DrawPolyLines(drawPos, 6, 35, 0, RAYWHITE);
            DrawTextEx(font, node.name.c_str(), { drawPos.x - 28, drawPos.y - 8 }, 12, 1, node.unlocked ? BLACK : WHITE);

            if (!node.unlocked) {
                DrawTextEx(font, TextFormat("SP:%d", node.cost), { drawPos.x - 15, drawPos.y + 15 }, 10, 1, WHITE);
            }

            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea)) {
                if (CheckCollisionPointCircle(GetMousePosition(), drawPos, 35)) {
                    hoveredSkillId = i;
                    if (available && clickInput) p.UnlockSkill(node.id);
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
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r) && clickInput) { itemSubTab = i; itemPage = 0; }
            DrawRectangleRec(r, c); std::string label = T(subK[i], subK[i]); DrawTextEx(font, label.c_str(), { r.x + 10, r.y + 8 }, 16, 1, WHITE);
            UI::RegisterInteractable(r);
        }
        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL"; for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perP = 10; int maxP = (int)ceil((float)filtered.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(T("PAGE_INFO", "Page %d/%d").c_str(), itemPage + 1, maxP), { 600, 125 }, 18, 1, WHITE);
        for (int i = 0; i < perP; i++) {
            int lIdx = itemPage * perP + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 165 + i * 42; Rectangle itemRect = { 120, (float)y, 400, 38 };

            UI::RegisterInteractable(itemRect);

            DrawRectangleRec(itemRect, Fade(BLACK, showDetail ? 0.2f : 0.4f));
            DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), { 135, (float)y + 10 }, 18, 1.0f, Player::GetItemRarityColor(item));
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), itemRect)) { if (clickInput) OpenDetail(item); }

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
        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea) && rightDownInput) {
            Vector2 delta = GetMouseDelta();
            if (IsGamepadAvailable(0)) {
                delta.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 10.0f;
                delta.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 10.0f;
            }
            mapOffset = Vector2Add(mapOffset, delta);
        }
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
        // --- 左カラム: サウンド設定 ---
        DrawTextEx(font, T("SOUND_SETTING", "Sound Settings").c_str(), { 150, 130 }, 24, 1, WHITE);

        int bgmVolInt = (int)roundf(AudioManager::bgmVolume * 100.0f);
        DrawTextEx(font, TextFormat(T("VOL_BGM", "BGM Volume: %d").c_str(), bgmVolInt), { 150, 180 }, 20, 1, WHITE);

        Rectangle bgmBar = { 150, 210, 300, 20 };
        UI::RegisterInteractable(bgmBar);

        DrawRectangleRec(bgmBar, GRAY);
        DrawRectangle(bgmBar.x, bgmBar.y, bgmBar.width * AudioManager::bgmVolume, bgmBar.height, GREEN);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 200, 300, 40 }) && downInput) {
            float newVol = (GetMouseX() - 150) / 300.0f;
            AudioManager::SetBGMVolume(newVol);
            DataManager::SaveConfig();
        }

        if (UI::DrawButton({ 470, 200, 40, 40 }, "-", font, GRAY)) {
            AudioManager::SetBGMVolume(AudioManager::bgmVolume - 0.05f);
            DataManager::SaveConfig();
        }
        if (UI::DrawButton({ 520, 200, 40, 40 }, "+", font, GRAY)) {
            AudioManager::SetBGMVolume(AudioManager::bgmVolume + 0.05f);
            DataManager::SaveConfig();
        }

        int seVolInt = (int)roundf(AudioManager::seVolume * 100.0f);
        DrawTextEx(font, TextFormat(T("VOL_SE", "SE Volume: %d").c_str(), seVolInt), { 150, 270 }, 20, 1, WHITE);

        Rectangle seBar = { 150, 300, 300, 20 };
        UI::RegisterInteractable(seBar);

        DrawRectangleRec(seBar, GRAY);
        DrawRectangle(seBar.x, seBar.y, seBar.width * AudioManager::seVolume, seBar.height, ORANGE);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 290, 300, 40 }) && downInput) {
            float newVol = (GetMouseX() - 150) / 300.0f;
            AudioManager::SetSEVolume(newVol);
            DataManager::SaveConfig();
            if (clickInput) AudioManager::PlaySE(SE_CLICK);
        }

        if (UI::DrawButton({ 470, 290, 40, 40 }, "-", font, GRAY)) {
            AudioManager::SetSEVolume(AudioManager::seVolume - 0.05f);
            DataManager::SaveConfig();
            AudioManager::PlaySE(SE_CLICK);
        }
        if (UI::DrawButton({ 520, 290, 40, 40 }, "+", font, GRAY)) {
            AudioManager::SetSEVolume(AudioManager::seVolume + 0.05f);
            DataManager::SaveConfig();
            AudioManager::PlaySE(SE_CLICK);
        }

        // --- 右カラム: 操作・画面設定 ---
        DrawTextEx(font, T("CTRL_SCREEN_SETTING", "Control & Screen").c_str(), { 650, 130 }, 24, 1, WHITE);

        DrawTextEx(font, TextFormat(T("SENS_MOUSE", "Mouse Sensitivity: %.1f").c_str(), DataManager::keyConfig.mouseSensitivity), { 650, 180 }, 20, 1, WHITE);
        Rectangle mSensBar = { 650, 210, 300, 20 };
        UI::RegisterInteractable(mSensBar);
        DrawRectangleRec(mSensBar, GRAY);
        float mRatio = (DataManager::keyConfig.mouseSensitivity - 0.1f) / 4.9f;
        if (mRatio < 0) mRatio = 0; if (mRatio > 1) mRatio = 1;
        DrawRectangle(mSensBar.x, mSensBar.y, mSensBar.width * mRatio, mSensBar.height, SKYBLUE);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 650, 200, 300, 40 }) && downInput) {
            float newRatio = (GetMouseX() - 650) / 300.0f;
            if (newRatio < 0) newRatio = 0; if (newRatio > 1) newRatio = 1;
            DataManager::keyConfig.mouseSensitivity = 0.1f + newRatio * 4.9f;
            DataManager::SaveConfig();
        }
        if (UI::DrawButton({ 970, 200, 40, 40 }, "-", font, GRAY)) {
            DataManager::keyConfig.mouseSensitivity -= 0.1f;
            if (DataManager::keyConfig.mouseSensitivity < 0.1f) DataManager::keyConfig.mouseSensitivity = 0.1f;
            DataManager::SaveConfig();
        }
        if (UI::DrawButton({ 1020, 200, 40, 40 }, "+", font, GRAY)) {
            DataManager::keyConfig.mouseSensitivity += 0.1f;
            if (DataManager::keyConfig.mouseSensitivity > 5.0f) DataManager::keyConfig.mouseSensitivity = 5.0f;
            DataManager::SaveConfig();
        }

        DrawTextEx(font, TextFormat(T("SENS_PAD", "Pad Sensitivity: %.1f").c_str(), DataManager::keyConfig.padSensitivity), { 650, 270 }, 20, 1, WHITE);
        Rectangle pSensBar = { 650, 300, 300, 20 };
        UI::RegisterInteractable(pSensBar);
        DrawRectangleRec(pSensBar, GRAY);
        float pRatio = (DataManager::keyConfig.padSensitivity - 0.1f) / 4.9f;
        if (pRatio < 0) pRatio = 0; if (pRatio > 1) pRatio = 1;
        DrawRectangle(pSensBar.x, pSensBar.y, pSensBar.width * pRatio, pSensBar.height, PURPLE);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 650, 290, 300, 40 }) && downInput) {
            float newRatio = (GetMouseX() - 650) / 300.0f;
            if (newRatio < 0) newRatio = 0; if (newRatio > 1) newRatio = 1;
            DataManager::keyConfig.padSensitivity = 0.1f + newRatio * 4.9f;
            DataManager::SaveConfig();
        }
        if (UI::DrawButton({ 970, 290, 40, 40 }, "-", font, GRAY)) {
            DataManager::keyConfig.padSensitivity -= 0.1f;
            if (DataManager::keyConfig.padSensitivity < 0.1f) DataManager::keyConfig.padSensitivity = 0.1f;
            DataManager::SaveConfig();
        }
        if (UI::DrawButton({ 1020, 290, 40, 40 }, "+", font, GRAY)) {
            DataManager::keyConfig.padSensitivity += 0.1f;
            if (DataManager::keyConfig.padSensitivity > 5.0f) DataManager::keyConfig.padSensitivity = 5.0f;
            DataManager::SaveConfig();
        }

        // --- 画面モード（フルスクリーン） ---
        DrawTextEx(font, T("SCREEN_MODE", "Screen Mode").c_str(), { 650, 370 }, 20, 1, WHITE);

        bool isFS = DataManager::keyConfig.isFullscreen;
        if (UI::DrawButton({ 650, 410, 150, 40 }, T("WINDOWED", "Windowed").c_str(), font, isFS ? DARKGRAY : BLUE)) {
            if (isFS) {
                ToggleFullscreen();
                DataManager::keyConfig.isFullscreen = false;
                DataManager::SaveConfig();
                AudioManager::PlaySE(SE_CLICK);
            }
        }
        if (UI::DrawButton({ 820, 410, 150, 40 }, T("FULLSCREEN", "Fullscreen").c_str(), font, isFS ? BLUE : DARKGRAY)) {
            if (!isFS) {
                ToggleFullscreen();
                DataManager::keyConfig.isFullscreen = true;
                DataManager::SaveConfig();
                AudioManager::PlaySE(SE_CLICK);
            }
        }

        // リセットボタン
        if (UI::DrawButton({ (float)sw - 350, (float)sh - 170, 200, 50 }, T("RESET_CONFIG", "Reset Settings").c_str(), font, MAROON)) {
            DataManager::ResetConfig();
            // 初期状態(ウィンドウモード)と現在の状態が食い違っているなら直す
            if (DataManager::keyConfig.isFullscreen && !IsWindowFullscreen()) ToggleFullscreen();
            if (!DataManager::keyConfig.isFullscreen && IsWindowFullscreen()) ToggleFullscreen();
            AudioManager::PlaySE(SE_SAVE);
        }
        }
    else if (tab == CONTROL_TAB) {
        static int waitingForKeyIndex = -1;
        static bool waitingForPad = false;

        DrawTextEx(font, T("KEY_CONFIG", "Key Configuration").c_str(), { 150, 130 }, 24, 1, WHITE);

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
                        *binds[waitingForKeyIndex].padPtr = i;
                        waitingForKeyIndex = -1;
                        DataManager::SaveConfig();
                        AudioManager::PlaySE(SE_CLICK);
                        break;
                    }
                }
            }
            else {
                int keyPressed = GetKeyPressed();
                if (keyPressed > 0) {
                    *binds[waitingForKeyIndex].keyPtr = keyPressed;
                    waitingForKeyIndex = -1;
                    DataManager::SaveConfig();
                    AudioManager::PlaySE(SE_CLICK);
                }
            }
        }
        else {
            for (int i = 0; i < 12; i++) {
                int col = i / 6;
                int row = i % 6;
                float x = 120.0f + col * 450.0f;
                float y = 180.0f + row * 60.0f;

                DrawTextEx(font, T(binds[i].tKey, binds[i].label).c_str(), { x, y + 10 }, 20, 1, LIGHTGRAY);

                if (binds[i].keyPtr != nullptr) {
                    std::string keyName = "KEY_" + std::to_string(*binds[i].keyPtr);
                    if (*binds[i].keyPtr >= 32 && *binds[i].keyPtr <= 126) keyName = std::string(1, (char)*binds[i].keyPtr);
                    else if (*binds[i].keyPtr == KEY_LEFT_SHIFT || *binds[i].keyPtr == KEY_RIGHT_SHIFT) keyName = "SHIFT";
                    else if (*binds[i].keyPtr == KEY_SPACE) keyName = "SPACE";

                    Rectangle btnR = { x + 160, y, 90, 40 };
                    if (UI::DrawButton(btnR, keyName.c_str(), font, DARKGRAY)) {
                        waitingForKeyIndex = i; waitingForPad = false; AudioManager::PlaySE(SE_CLICK);
                    }
                }
                else {
                    DrawTextEx(font, T("LCLICK", "L-Click").c_str(), { x + 175, y + 10 }, 16, 1, GRAY);
                }

                if (binds[i].padPtr != nullptr) {
                    std::string padName = UI::GetPadBtnStr(*binds[i].padPtr);
                    Rectangle btnR = { x + 260, y, 90, 40 };
                    if (UI::DrawButton(btnR, padName.c_str(), font, Fade(DARKBLUE, 0.8f))) {
                        waitingForKeyIndex = i; waitingForPad = true; AudioManager::PlaySE(SE_CLICK);
                    }
                }
                else {
                    DrawTextEx(font, T("LSTICK", "L-Stick").c_str(), { x + 275, y + 10 }, 16, 1, GRAY);
                }
            }

            // ★追加: リセットボタン (キー割り当て画面からも初期化できるようにする)
            if (UI::DrawButton({ (float)sw - 350, (float)sh - 170, 200, 50 }, T("RESET_CONFIG", "Reset Settings").c_str(), font, MAROON)) {
                DataManager::ResetConfig();
                AudioManager::PlaySE(SE_SAVE);
            }
        }
    }

    DrawDetailWindow(font);
    return eventCode;
}