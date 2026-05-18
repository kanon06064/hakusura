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

// メインメニューの描画と入力判定を行う関数
int UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab, Font font) {
    int eventCode = 0; // 1=セーブ実行, 2=タイトルへ戻る

    // 現在のウィンドウ(またはモニター)の解像度を取得して、UIを相対的に配置する
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(100, 50, sw - 200, sh - 100, Fade(DARKGRAY, 0.95f)); // 背景の半透明の板

    // --- 上部のタブ切り替えボタン ---
    const char* tKeys[] = { "EQUIP", "SKILL", "MAP", "ITEMS", "SYSTEM", "OPTION", "CONTROL" };
    const char* tDefs[] = { "Equip", "Skill", "Map", "Items", "System", "Option", "Control" };

    for (int i = 0; i < 7; i++) {
        Rectangle r = { 100.0f + (float)i * 140, 70.0f, 125.0f, 40.0f };
        Color tabColor = (tab == i) ? BLUE : DARKGRAY;
        if (UI::DrawButton(r, T(tKeys[i], tDefs[i]).c_str(), font, tabColor)) { tab = (MenuTab)i; }
    }

    bool clickInput = IsMouseButtonPressed(0) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool downInput = IsMouseButtonDown(0) || IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    bool rightDownInput = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) || IsGamepadButtonDown(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_2);

    // ==========================================
    // タブごとの画面描画処理
    // ==========================================
    if (tab == EQUIP) {
        // --- 装備タブ (左：現在装備、中央：防具、右：所持装備) ---
        float leftX = 120.0f;
        float midX = (float)sw / 2.0f - 160.0f; // 画面中央付近
        float rightX = (float)sw - 560.0f;      // 画面右側付近

        // 【左側】武器スロット (2つ)
        DrawTextEx(font, T("ACTIVE_SLOTS", "Equipped").c_str(), { leftX, 130 }, 20, 1, GOLD);
        for (int i = 0; i < 2; i++) {
            int y = 160 + i * 105; bool isEmpty = (p.equippedData[i].id == -1);
            Rectangle slotRect = { leftX, (float)y, 260, 95 }; Rectangle btnRect = { leftX + 180, (float)y + 25, 70, 40 };
            Color slotCol = (p.activeSlot == i) ? MAROON : BLACK; if (showDetail) slotCol = ColorBrightness(slotCol, -0.4f);

            UI::RegisterInteractable(slotRect);

            DrawRectangleRec(slotRect, slotCol);
            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                // ボタン部分以外の領域をクリックしたら詳細画面(Detail)を開く
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) { if (clickInput) OpenDetail(p.equippedData[i]); }
            }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedData[i]).c_str(), { leftX + 10, (float)y + 25 }, 20, 1, Player::GetItemRarityColor(p.equippedData[i]));
                float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[i]);
                DrawTextEx(font, TextFormat("%s +%.1f", T("ATK", "ATK").c_str(), totalBonus), { leftX + 10, (float)y + 50 }, 14, 1, YELLOW);
                if (UI::DrawButton(btnRect, T("OFF", "OFF").c_str(), font, RED)) p.UnequipWeapon(i); // 外すボタン
            }
            else DrawTextEx(font, T("EMPTY", "EMPTY").c_str(), { leftX + 10, (float)y + 35 }, 20, 1, DARKGRAY);
        }

        // 【中央】防具スロット (頭、胴、腕、脚、足の5部位)
        const char* armorKeys[] = { "HEAD", "CHEST", "HANDS", "LEGS", "FEET" };
        const char* armorNames[] = { "Head", "Chest", "Hands", "Legs", "Feet" };
        for (int i = 0; i < 5; i++) {
            int y = 160 + i * 70; DrawTextEx(font, T(armorKeys[i], armorNames[i]).c_str(), { midX - 60, (float)y + 20 }, 16, 1, LIGHTGRAY);
            bool isEmpty = (p.equippedArmor[i].id == -1);
            Rectangle slotRect = { midX, (float)y, 200, 60 }; Rectangle btnRect = { midX + 130, (float)y + 10, 70, 40 };

            UI::RegisterInteractable(slotRect);

            DrawRectangleRec(slotRect, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK); DrawRectangleLinesEx(slotRect, 1, showDetail ? GRAY : DARKGRAY);
            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) { if (clickInput) OpenDetail(p.equippedArmor[i]); }
            }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedArmor[i]).c_str(), { midX + 10, (float)y + 10 }, 14, 1, Player::GetItemRarityColor(p.equippedArmor[i]));
                float def = p.equippedArmor[i].defBonus + DataManager::GetModifier(p.equippedArmor[i].modifierId).def;
                DrawTextEx(font, TextFormat("%s +%.1f", T("DEF", "DEF").c_str(), def), { midX + 10, (float)y + 35 }, 12, 1, BLUE);
                if (UI::DrawButton(btnRect, T("OFF", "OUT").c_str(), font, RED)) p.UnequipArmor(i);
            }
            else { DrawTextEx(font, T("EMPTY", "EMPTY").c_str(), { midX + 10, (float)y + 20 }, 14, 1, DARKGRAY); }
        }

        // 【右側】手持ちの未装備品リスト (ページめくり対応)
        DrawTextEx(font, T("OWNED_EQUIP", "Owned Equipment").c_str(), { rightX, 130 }, 18, 1, GOLD);
        const int perP = 8; int maxP = (int)ceil((float)p.inventoryEquip.size() / perP); if (maxP < 1) maxP = 1;
        for (int i = 0; i < perP; i++) {
            int idx = equipPage * perP + i; if (idx >= (int)p.inventoryEquip.size()) break;
            int y = 160 + i * 45; Rectangle r = { rightX, (float)y, 300, 40 };

            UI::RegisterInteractable(r);

            DrawRectangleRec(r, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r)) { if (GetMouseX() < rightX + 230) { if (clickInput) OpenDetail(p.inventoryEquip[idx]); } }
            DrawTextEx(font, Player::GetFullItemName(p.inventoryEquip[idx]).c_str(), { rightX + 10, (float)y + 10 }, 14, 1, Player::GetItemRarityColor(p.inventoryEquip[idx]));

            if (p.inventoryEquip[idx].type == "EQUIP") {
                // 武器は スロット1(W1) か スロット2(W2) を選んで装備
                if (UI::DrawButton({ rightX + 220, (float)y, 40, 40 }, T("W1", "W1").c_str(), font, DARKGRAY)) { p.EquipWeapon(idx, 0); break; }
                if (UI::DrawButton({ rightX + 265, (float)y, 40, 40 }, T("W2", "W2").c_str(), font, DARKGRAY)) { p.EquipWeapon(idx, 1); break; }
            }
            else if (p.inventoryEquip[idx].type == "ARMOR") {
                // 防具は種類(部位)が合致する枠へ装備
                int subtype = p.inventoryEquip[idx].weaponSubtype;
                if (subtype >= 0 && subtype < 5) {
                    if (UI::DrawButton({ rightX + 220, (float)y, 85, 40 }, T("EQUIP", "EQUIP").c_str(), font, DARKGREEN)) { p.EquipArmor(idx, subtype); break; }
                }
            }
        }
        if (UI::DrawButton({ rightX, 530, 80, 30 }, "<<", font, GRAY) && equipPage > 0) equipPage--;
        if (UI::DrawButton({ rightX + 90, 530, 80, 30 }, ">>", font, GRAY) && equipPage < maxP - 1) equipPage++;
    }
    else if (tab == SKILL) {
        // --- スキルツリータブ ---
        Rectangle viewArea = { 100, 120, (float)sw - 200, (float)sh - 170 }; // ツリーを描画する枠
        DrawTextEx(font, T("CAM_CONTROL", "Right Click & Drag to Move").c_str(), { 120, 620 }, 16, 1, LIGHTGRAY);

        // 右ドラッグでスキルツリーの画面全体を移動させる
        if (rightDownInput && !showDetail) {
            Vector2 delta = GetMouseDelta();
            if (IsGamepadAvailable(0)) { // パッド操作時
                delta.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 10.0f;
                delta.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 10.0f;
            }
            skillOffset = Vector2Add(skillOffset, delta);
        }

        // ScissorModeを使って、枠(viewArea)の外にはみ出た要素を描画させないようにする
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);

        // ノード同士を繋ぐ線(前提スキルライン)を先に描画
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

        // ノード(六角形アイコン)本体の描画
        for (int i = 0; i < (int)p.skillTree.size(); i++) {
            auto& node = p.skillTree[i];
            bool available = p.IsSkillAvailable(node.id);
            Vector2 drawPos = Vector2Add(node.uiPos, skillOffset);
            Color nodeColor = node.unlocked ? YELLOW : (available ? GREEN : DARKGRAY);

            if (node.type != SKILL_PASSIVE) { // アクティブスキルは色を変える
                nodeColor = node.unlocked ? ORANGE : (available ? PURPLE : DARKGRAY);
            }

            Rectangle nodeRect = { drawPos.x - 35, drawPos.y - 35, 70, 70 };
            UI::RegisterInteractable(nodeRect); // パッドのスナップ用

            DrawPoly(drawPos, 6, 35, 0, nodeColor);
            DrawPolyLines(drawPos, 6, 35, 0, RAYWHITE);
            DrawTextEx(font, node.name.c_str(), { drawPos.x - 28, drawPos.y - 8 }, 12, 1, node.unlocked ? BLACK : WHITE);

            if (!node.unlocked) { DrawTextEx(font, TextFormat("SP:%d", node.cost), { drawPos.x - 15, drawPos.y + 15 }, 10, 1, WHITE); }

            // クリック判定
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea)) {
                if (CheckCollisionPointCircle(GetMousePosition(), drawPos, 35)) {
                    hoveredSkillId = i;
                    if (available && clickInput) p.UnlockSkill(node.id);
                }
            }
        }
        EndScissorMode(); // 枠の制限を解除

        // スキルにマウスを乗せている時、ポップアップで説明を表示する
        if (hoveredSkillId != -1) {
            auto& node = p.skillTree[hoveredSkillId];
            Vector2 mPos = GetMousePosition();
            DrawRectangle(mPos.x + 15, mPos.y + 15, 250, 100, Fade(BLACK, 0.9f));
            DrawRectangleLines(mPos.x + 15, mPos.y + 15, 250, 100, GOLD);
            DrawTextEx(font, node.name.c_str(), { mPos.x + 25, mPos.y + 25 }, 18, 1, WHITE);
            DrawTextEx(font, TextFormat(T("SKILL_COST", "Cost: %d SP").c_str(), node.cost), { mPos.x + 25, mPos.y + 45 }, 14, 1, YELLOW);
            DrawTextEx(font, node.desc.c_str(), { mPos.x + 25, mPos.y + 65 }, 12, 1, LIGHTGRAY);

            if (node.unlocked) DrawTextEx(font, T("SKILL_UNLOCKED", "UNLOCKED").c_str(), { mPos.x + 160, mPos.y + 25 }, 14, 1, GREEN);
            else if (p.IsSkillAvailable(node.id)) DrawTextEx(font, T("SKILL_AVAILABLE", "AVAILABLE").c_str(), { mPos.x + 150, mPos.y + 25 }, 14, 1, SKYBLUE);
            else DrawTextEx(font, T("SKILL_LOCKED", "LOCKED").c_str(), { mPos.x + 170, mPos.y + 25 }, 14, 1, RED);
        }
    }
    else if (tab == INVENTORY) {
        // --- 手持ちアイテム(消耗品・素材)タブ ---
        const char* subK[] = { "CONSUMABLE", "MATERIAL" };
        for (int i = 0; i < 2; i++) { // サブタブの切り替えボタン
            Rectangle r = { 120.0f + (float)i * 210, 120, 200, 35 }; Color c = (itemSubTab == i) ? GREEN : BLACK; if (showDetail) c = ColorBrightness(c, -0.4f);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r) && clickInput) { itemSubTab = i; itemPage = 0; }
            DrawRectangleRec(r, c); std::string label = T(subK[i], subK[i]); DrawTextEx(font, label.c_str(), { r.x + 10, r.y + 8 }, 16, 1, WHITE);
            UI::RegisterInteractable(r);
        }

        // 表示するカテゴリでリストを絞り込む
        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL"; for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perP = 10; int maxP = (int)ceil((float)filtered.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(T("PAGE_INFO", "Page %d/%d").c_str(), itemPage + 1, maxP), { (float)sw / 2.0f, 125 }, 18, 1, WHITE);

        float listW = (float)sw / 2.0f - 100.0f; // ウィンドウサイズに応じたリストの幅
        for (int i = 0; i < perP; i++) {
            int lIdx = itemPage * perP + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 165 + i * 42; Rectangle itemRect = { 120, (float)y, listW, 38 };

            UI::RegisterInteractable(itemRect);

            DrawRectangleRec(itemRect, Fade(BLACK, showDetail ? 0.2f : 0.4f));
            DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), { 135, (float)y + 10 }, 18, 1.0f, Player::GetItemRarityColor(item));
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), itemRect)) { if (clickInput) OpenDetail(item); }

            if (itemSubTab == 0 && UI::DrawButton({ 120 + listW + 10, (float)y, 80, 38 }, T("USE", "Use").c_str(), font, GREEN)) {
                p.UseItem(invIdx); // 回復アイテムを使う
                break;
            }
        }
        if (UI::DrawButton({ 120, (float)sh - 120, 100, 30 }, "<<", font, GRAY) && itemPage > 0) itemPage--;
        if (UI::DrawButton({ 230, (float)sh - 120, 100, 30 }, ">>", font, GRAY) && itemPage < maxP - 1) itemPage++;
    }
    else if (tab == MAP_TAB) {
        // --- 全体マップタブ ---
        Rectangle viewArea = { 110, 120, (float)sw - 220, (float)sh - 170 };
        DrawTextEx(font, T("MAP_CONTROL", "Right Click & Drag to Move").c_str(), { 120, (float)sh - 100 }, 16, 1, LIGHTGRAY);
        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea) && rightDownInput) {
            Vector2 delta = GetMouseDelta();
            if (IsGamepadAvailable(0)) {
                delta.x = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_X) * 10.0f;
                delta.y = GetGamepadAxisMovement(0, GAMEPAD_AXIS_RIGHT_Y) * 10.0f;
            }
            mapOffset = Vector2Add(mapOffset, delta); // マップをドラッグで動かす
        }
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
        float sc = 12.0f; // マス目のサイズ
        float offX = (viewArea.x + viewArea.width / 2.0f) - (d.currentWidth * sc / 2.0f) + mapOffset.x; float offY = (viewArea.y + viewArea.height / 2.0f) - (d.currentHeight * sc / 2.0f) + mapOffset.y;
        DrawRectangle(offX - 5, offY - 5, d.currentWidth * sc + 10, d.currentHeight * sc + 10, BLACK); // マップ背景

        for (int y = 0; y < d.currentHeight; y++) {
            for (int x = 0; x < d.currentWidth; x++) {
                if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) {
                    DrawRectangle(offX + x * sc, offY + y * sc, sc - 1, sc - 1, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
                }
            }
        }
        DrawCircle(offX + (p.position.x / TILE_SIZE) * sc, offY + (p.position.z / TILE_SIZE) * sc, 5, RED); // プレイヤー位置
        EndScissorMode();
    }
    else if (tab == SYSTEM_TAB) {
        // --- システムタブ(セーブ・タイトルへ戻る) ---
        float centerX = sw / 2.0f;
        DrawTextEx(font, T("SYS_MENU", "System Menu").c_str(), { centerX - 100, 130 }, 24, 1, WHITE);

        Rectangle saveBtn = { centerX - 100, 200, 200, 60 };
        if (!d.isHome) { DrawRectangleRec(saveBtn, GRAY); DrawTextEx(font, T("SAVE_HOME_ONLY", "Save (Home Only)").c_str(), { centerX - 90, 220 }, 18, 1, DARKGRAY); } // ダンジョン内ではセーブ不可
        else { if (UI::DrawButton(saveBtn, T("SAVE_GAME", "Save Game").c_str(), font, BLUE)) { eventCode = 1; } DrawTextEx(font, T("SAVE_DESC", "Save your progress").c_str(), { centerX + 120, 220 }, 18, 1, LIGHTGRAY); }

        Rectangle titleBtn = { centerX - 100, 300, 200, 60 }; if (UI::DrawButton(titleBtn, T("RETURN_TITLE", "Return to Title").c_str(), font, RED)) { eventCode = 2; }
    }
    else if (tab == OPTION_TAB) {
        // --- オプションタブ (サウンド・画面・感度) ---
        float rightColX = (float)sw - 630.0f;

        // 【左カラム: サウンド設定】
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
            if (clickInput) AudioManager::PlaySE(SE_CLICK); // 調整時に音を鳴らす
        }

        if (UI::DrawButton({ 470, 290, 40, 40 }, "-", font, GRAY)) { AudioManager::SetSEVolume(AudioManager::seVolume - 0.05f); DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK); }
        if (UI::DrawButton({ 520, 290, 40, 40 }, "+", font, GRAY)) { AudioManager::SetSEVolume(AudioManager::seVolume + 0.05f); DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK); }

        // 【右カラム: 視点感度と画面設定】
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

        // --- 画面モード（フルスクリーン切替） ---
        DrawTextEx(font, T("SCREEN_MODE", "Screen Mode").c_str(), { rightColX, 370 }, 20, 1, WHITE);
        bool isFS = DataManager::keyConfig.isFullscreen;
        if (UI::DrawButton({ rightColX, 410, 150, 40 }, T("WINDOWED", "Windowed").c_str(), font, isFS ? DARKGRAY : BLUE)) {
            if (isFS) {
                ToggleFullscreen(); SetWindowSize(1280, 720); // ウィンドウサイズを戻す
                DataManager::keyConfig.isFullscreen = false; DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK);
            }
        }
        if (UI::DrawButton({ rightColX + 170, 410, 150, 40 }, T("FULLSCREEN", "Fullscreen").c_str(), font, isFS ? BLUE : DARKGRAY)) {
            if (!isFS) {
                int monitor = GetCurrentMonitor(); SetWindowSize(GetMonitorWidth(monitor), GetMonitorHeight(monitor)); // ネイティブ解像度に合わせる
                ToggleFullscreen();
                DataManager::keyConfig.isFullscreen = true; DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK);
            }
        }

        // 初期化(リセット)ボタン
        if (UI::DrawButton({ (float)sw - 350, (float)sh - 170, 200, 50 }, T("RESET_CONFIG", "Reset Settings").c_str(), font, MAROON)) {
            DataManager::ResetConfig();
            if (IsWindowFullscreen()) { ToggleFullscreen(); SetWindowSize(1280, 720); } // フルスクリーン解除
            AudioManager::PlaySE(SE_SAVE);
        }
    }
    else if (tab == CONTROL_TAB) {
        // --- キーコンフィグタブ ---
        static int waitingForKeyIndex = -1; // -1: 待機していない, それ以外: 割り当て入力待ちのキーインデックス
        static bool waitingForPad = false;

        float centerX = sw / 2.0f;
        DrawTextEx(font, T("KEY_CONFIG", "Key Configuration").c_str(), { centerX - 250, 130 }, 24, 1, WHITE);

        // コンフィグとして変更可能なキー一覧
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
            // キーやボタンの入力待ち状態
            DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));
            DrawTextEx(font, T("PRESS_ANY_KEY", "Press any key to assign...").c_str(), { (float)sw / 2 - 200, (float)sh / 2 - 20 }, 24, 1, YELLOW);

            if (waitingForPad) {
                for (int i = 1; i < 32; i++) { // パッドのボタン走査
                    if (IsGamepadButtonPressed(0, i)) {
                        *binds[waitingForKeyIndex].padPtr = i; waitingForKeyIndex = -1; DataManager::SaveConfig(); AudioManager::PlaySE(SE_CLICK); break;
                    }
                }
            }
            else {
                int keyPressed = GetKeyPressed(); // キーボード走査
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

            // オプションタブと同じリセットボタン
            if (UI::DrawButton({ (float)sw - 350, (float)sh - 170, 200, 50 }, T("RESET_CONFIG", "Reset Settings").c_str(), font, MAROON)) {
                DataManager::ResetConfig();
                if (IsWindowFullscreen()) { ToggleFullscreen(); SetWindowSize(1280, 720); }
                AudioManager::PlaySE(SE_SAVE);
            }
        }
    }

    DrawDetailWindow(font);
    return eventCode;
}