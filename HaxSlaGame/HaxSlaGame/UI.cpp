#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "raymath.h"
#include <math.h>

int UI::itemPage = 0; int UI::equipPage = 0; int UI::debugPage = 0;
int UI::storageInvPage = 0; int UI::storageBoxPage = 0; int UI::itemSubTab = 0;
int UI::reforgeItemIdx = -1;
int UI::warpScroll = 0;
int UI::craftingScroll = 0;
Vector2 UI::skillOffset = { 0.0f, 0.0f };
Vector2 UI::mapOffset = { 0.0f, 0.0f };

bool UI::showDetail = false;
ItemData UI::focusingItem;
float UI::detailOpenTimer = 0.0f;
int UI::deleteConfirmSlot = 0;

void UI::OpenDetail(const ItemData& item) {
    focusingItem = item;
    showDetail = true;
    detailOpenTimer = 0.0f;
    AudioManager::PlaySE(SE_CLICK);
}

bool UI::DrawButton(Rectangle r, const char* label, Font font, Color col) {
    bool locked = showDetail;

    bool clicked = false;
    bool hover = !locked && CheckCollisionPointRec(GetMousePosition(), r);

    Color drawCol = locked ? ColorBrightness(col, -0.4f) : col;
    if (hover) drawCol = ColorBrightness(col, 0.2f);

    DrawRectangleRec(r, drawCol);
    DrawRectangleLinesEx(r, 2, locked ? GRAY : RAYWHITE);

    Vector2 tSize = MeasureTextEx(font, label, 18, 1);
    DrawTextEx(font, label, { r.x + r.width / 2 - tSize.x / 2, r.y + r.height / 2 - tSize.y / 2 }, 18, 1, locked ? LIGHTGRAY : WHITE);

    if (hover && IsMouseButtonPressed(0)) {
        clicked = true;
        AudioManager::PlaySE(SE_CLICK);
    }
    return clicked;
}

void UI::DrawDetailWindow(Font font) {
    if (!showDetail) return;
    detailOpenTimer += GetFrameTime();

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.7f));

    int w = 450;
    int h = 550;
    int x = (sw - w) / 2;
    int y = (sh - h) / 2;

    DrawRectangle(x, y, w, h, Fade(DARKBLUE, 0.95f));
    DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 3, GOLD);

    DrawTextEx(font, Player::GetFullItemName(focusingItem).c_str(), { (float)x + 25, (float)y + 25 }, 28, 1, GOLD);

    std::string typeStr = focusingItem.type;
    if (typeStr == "EQUIP") typeStr = u8"武器";
    else if (typeStr == "ARMOR") typeStr = u8"防具";
    else if (typeStr == "CONSUMABLE") typeStr = u8"消耗品";
    else if (typeStr == "MATERIAL") typeStr = u8"素材";

    if (focusingItem.type == "EQUIP") {
        const char* wTypes[] = { u8"剣", u8"槍", u8"斧", u8"弓", u8"杖", u8"なし" };
        if (focusingItem.weaponSubtype >= 0 && focusingItem.weaponSubtype <= 4) typeStr += std::string(" (") + wTypes[focusingItem.weaponSubtype] + ")";
    }
    else if (focusingItem.type == "ARMOR") {
        const char* aTypes[] = { u8"頭", u8"胴", u8"手", u8"脚", u8"足" };
        if (focusingItem.weaponSubtype >= 0 && focusingItem.weaponSubtype <= 4) typeStr += std::string(" (") + aTypes[focusingItem.weaponSubtype] + ")";
    }
    DrawTextEx(font, typeStr.c_str(), { (float)x + 25, (float)y + 60 }, 20, 1, LIGHTGRAY);

    Modifier mod = DataManager::GetModifier(focusingItem.modifierId);
    float totalAtk = focusingItem.atkBonus + mod.atk;
    float totalDef = focusingItem.defBonus + mod.def;
    float totalHp = focusingItem.hpBonus + mod.hp;
    float totalSpd = focusingItem.speedBonus + mod.spd;

    int statsY = y + 110;
    int lineH = 35;

    if (totalAtk != 0) { DrawTextEx(font, TextFormat(u8"攻撃力 : %+.1f", totalAtk), { (float)x + 40, (float)statsY }, 22, 1, RED); if (mod.atk != 0) DrawTextEx(font, TextFormat(u8"(補正 %+.1f)", mod.atk), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (totalDef != 0) { DrawTextEx(font, TextFormat(u8"防御力 : %+.1f", totalDef), { (float)x + 40, (float)statsY }, 22, 1, BLUE); if (mod.def != 0) DrawTextEx(font, TextFormat(u8"(補正 %+.1f)", mod.def), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (totalHp != 0) { DrawTextEx(font, TextFormat(u8"最大HP : %+.0f", totalHp), { (float)x + 40, (float)statsY }, 22, 1, GREEN); if (mod.hp != 0) DrawTextEx(font, TextFormat(u8"(補正 %+.0f)", mod.hp), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (totalSpd != 0) { DrawTextEx(font, TextFormat(u8"速度   : %+.2f", totalSpd), { (float)x + 40, (float)statsY }, 22, 1, SKYBLUE); if (mod.spd != 0) DrawTextEx(font, TextFormat(u8"(補正 %+.2f)", mod.spd), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (focusingItem.heal > 0) { DrawTextEx(font, TextFormat(u8"回復量 : %.0f", focusingItem.heal), { (float)x + 40, (float)statsY }, 22, 1, PINK); statsY += lineH; }

    if (mod.id != 0) {
        statsY += 20;
        DrawRectangleLines(x + 20, statsY - 5, w - 40, 70, ORANGE);
        DrawTextEx(font, u8"付与効果(エンチャント):", { (float)x + 30, (float)statsY }, 18, 1, ORANGE);
        DrawTextEx(font, mod.name.c_str(), { (float)x + 50, (float)statsY + 30 }, 22, 1, YELLOW);
    }

    Rectangle closeBtn = { (float)x + w / 2 - 80, (float)y + h - 70, 160, 50 };
    bool inputEnabled = (detailOpenTimer >= 0.3f);
    bool hover = inputEnabled && CheckCollisionPointRec(GetMousePosition(), closeBtn);

    Color btnCol = hover ? RED : MAROON;
    if (!inputEnabled) btnCol = Fade(MAROON, 0.5f);

    DrawRectangleRec(closeBtn, btnCol);
    DrawRectangleLinesEx(closeBtn, 2, WHITE);
    Vector2 txtSz = MeasureTextEx(font, u8"閉じる", 24, 1);
    DrawTextEx(font, u8"閉じる", { closeBtn.x + closeBtn.width / 2 - txtSz.x / 2, closeBtn.y + closeBtn.height / 2 - txtSz.y / 2 }, 24, 1, inputEnabled ? WHITE : GRAY);

    if (hover && IsMouseButtonPressed(0)) {
        showDetail = false;
        AudioManager::PlaySE(SE_CLICK);
    }
}

// 【変更】タイトル画面の描画処理を修正
int UI::DrawTitleScreen(Font font) {
    int sw = GetScreenWidth(); int sh = GetScreenHeight();

    // 画像がロードされている場合は背景として描画
    if (DataManager::titleBg.id != 0) {
        Rectangle source = { 0.0f, 0.0f, (float)DataManager::titleBg.width, (float)DataManager::titleBg.height };
        Rectangle dest = { 0.0f, 0.0f, (float)sw, (float)sh };
        Vector2 origin = { 0.0f, 0.0f };
        // 画像を画面いっぱいに描画
        DrawTexturePro(DataManager::titleBg, source, dest, origin, 0.0f, WHITE);
    }
    else {
        // 画像が見つからなかった場合のフォールバック（以前の表示）
        DrawRectangleGradientV(0, 0, sw, sh, DARKBLUE, BLACK);
        const char* title = "3D Hack & Slash";
        Vector2 tSize = MeasureTextEx(font, title, 60, 2);
        DrawTextEx(font, title, { (float)(sw - tSize.x) / 2, 100.0f }, 60, 2, GOLD);
    }

    // 削除確認ダイアログ
    if (deleteConfirmSlot > 0) {
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));
        int w = 500, h = 250;
        int x = (sw - w) / 2, y = (sh - h) / 2;

        DrawRectangle(x, y, w, h, DARKGRAY);
        DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 3, RED);

        DrawTextEx(font, TextFormat(u8"Slot %d のデータを削除しますか？", deleteConfirmSlot), { (float)x + 40, (float)y + 60 }, 24, 1, WHITE);
        DrawTextEx(font, u8"削除すると元に戻せません。", { (float)x + 80, (float)y + 100 }, 20, 1, RED);

        if (DrawButton({ (float)x + 50, (float)y + 160, 150, 50 }, u8"削除する", font, RED)) {
            DataManager::DeleteSave(deleteConfirmSlot);
            deleteConfirmSlot = 0;
        }
        if (DrawButton({ (float)x + 300, (float)y + 160, 150, 50 }, u8"キャンセル", font, GRAY)) {
            deleteConfirmSlot = 0;
        }
        return 0;
    }

    int selectedSlot = 0;
    // スロットボタンの位置を少し下にずらして、画像ロゴと被らないように調整
    for (int i = 1; i <= 3; i++) {
        SaveHeader h = DataManager::GetSaveHeader(i);
        float y = 350.0f + (float)(i - 1) * 100.0f; // Y座標を300から350に変更
        Rectangle r = { (float)sw / 2 - 200, y, 400.0f, 80.0f };
        std::string label; Color c;

        // 背景が画像になるため、ボタンを半透明にして馴染ませる
        if (h.exists) { label = TextFormat("Slot %d: Lv.%d  Floor %d", i, h.playerLevel, h.floor); c = Fade(DARKGREEN, 0.8f); }
        else { label = TextFormat("Slot %d: (NO DATA)", i); c = Fade(DARKGRAY, 0.8f); }

        if (DrawButton(r, label.c_str(), font, c)) { selectedSlot = i; }

        if (h.exists) {
            Rectangle delBtn = { r.x + r.width + 20, r.y + 20, 60, 40 };
            if (DrawButton(delBtn, u8"削除", font, Fade(MAROON, 0.8f))) {
                deleteConfirmSlot = i;
            }
        }
    }

    if (DrawButton({ 20, (float)sh - 60, 200, 40 }, u8"デバッグルーム", font, Fade(PURPLE, 0.8f))) {
        return 999;
    }

    return selectedSlot;
}

void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, bool debug, Font font) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    std::string floorText;
    if (floor == 0) floorText = "HOME"; else { std::string label = "Floor"; if (DataManager::uiStrings.count("FLOOR")) label = DataManager::uiStrings["FLOOR"]; floorText = TextFormat("%s %d", label.c_str(), floor); }
    Vector2 fSize = MeasureTextEx(font, floorText.c_str(), 24, 1); DrawRectangle(20, 20, (int)fSize.x + 30, 40, Fade(BLACK, 0.6f)); DrawTextEx(font, floorText.c_str(), { 35, 28 }, 24, 1, WHITE);

    int listCount = 0;
    for (auto& e : enemies) {
        if (e.hudTimer > 0) {
            int yPos = 80 + listCount * 50; DrawRectangle(sw - 220, yPos, 200, 45, Fade(BLACK, 0.7f));
            DrawTextEx(font, TextFormat("Lv.%d %s", e.level, e.data.name.c_str()), { (float)sw - 210, (float)yPos + 5 }, 16, 1, WHITE);
            DrawRectangle(sw - 210, yPos + 25, 180, 10, DARKGRAY); DrawRectangle(sw - 210, yPos + 25, (int)(180.0f * (e.hp / e.maxHp)), 10, RED); listCount++; if (listCount >= 5) break;
        }
        if (debug || d.IsDiscovered((float)e.position.x, (float)e.position.z)) {
            Vector2 s = GetWorldToScreen(e.position, cam);
            if (s.x > 0 && s.y > 0 && s.x < sw && s.y < sh) {
                std::string txt = "Lv." + std::to_string(e.level) + " " + e.data.name; Vector2 tSize = MeasureTextEx(font, txt.c_str(), 16, 1); DrawTextEx(font, txt.c_str(), { s.x - tSize.x / 2, s.y - 45 }, 16, 1, WHITE); DrawRectangle((int)s.x - 20, (int)s.y - 25, 40, 4, DARKGRAY); DrawRectangle((int)s.x - 20, (int)s.y - 25, (int)(40.0f * (e.hp / e.maxHp)), 4, RED);
            }
        }
    }

    DrawRectangle(10, sh - 120, 320, 110, Fade(BLACK, 0.6f));
    DrawTextEx(font, TextFormat("Lv: %d   EXP: %d/%d", p.level, p.exp, p.expToNext), { 20, (float)sh - 110 }, 18, 1, SKYBLUE);
    DrawRectangle(20, sh - 85, 280, 18, DARKGRAY); DrawRectangle(20, sh - 85, (int)(280 * (fmaxf(0.0f, p.hp) / p.maxHp)), 18, GREEN);
    DrawTextEx(font, TextFormat("HP: %.0f/%.0f", p.hp, p.maxHp), { 30, (float)sh - 84 }, 14, 1, WHITE);
    DrawTextEx(font, TextFormat("ATK: %.1f  DEF: %.1f", p.attackPower, p.defense), { 20, (float)sh - 60 }, 18, 1, WHITE);
    DrawTextEx(font, TextFormat("Gold: %d  SP: %d", p.gold, p.skillPoints), { 20, (float)sh - 35 }, 18, 1, WHITE);

    int iconSize = 40; int startX = sw - 150; int startY = sh - 60;
    struct SkillIcon { SkillType type; const char* label; const char* key; };
    SkillIcon icons[] = { { SKILL_ACTIVE_DASH, u8"ダッシュ", "1" }, { SKILL_ACTIVE_SMASH, u8"強撃", "2" }, { SKILL_ACTIVE_STEALTH, u8"隠密", "3" } };
    for (int i = 0; i < 3; i++) {
        int x = startX + i * (iconSize + 10); bool unlocked = p.IsSkillUnlocked(icons[i].type); Color baseCol = unlocked ? DARKBLUE : DARKGRAY;
        DrawRectangle(x, startY, iconSize, iconSize, baseCol); DrawRectangleLines(x, startY, iconSize, iconSize, RAYWHITE); DrawTextEx(font, icons[i].key, { (float)x + 2, (float)startY + 2 }, 10, 1, WHITE);
        if (unlocked) {
            float cd = p.GetSkillCooldown(icons[i].type); float maxCd = p.GetSkillMaxCooldown(icons[i].type);
            if (cd > 0) { float ratio = cd / maxCd; DrawRectangle(x, startY + (int)((float)iconSize * (1.0f - ratio)), iconSize, (int)((float)iconSize * ratio), Fade(RED, 0.7f)); DrawTextEx(font, TextFormat("%.1f", cd), { (float)x + 5, (float)startY + 15 }, 14, 1, YELLOW); }
            else { std::string label = icons[i].label; if (DataManager::uiStrings.count(label)) label = DataManager::uiStrings[label]; DrawTextEx(font, label.c_str(), { (float)x + 2, (float)startY + 25 }, 10, 1, GREEN); }
        }
        else { DrawTextEx(font, "LOCK", { (float)x + 5, (float)startY + 15 }, 10, 1, GRAY); }
    }
}

void UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab, Font font) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(100, 50, sw - 200, sh - 100, Fade(DARKGRAY, 0.95f));
    const char* tKeys[] = { "EQUIP", "SKILL", "MAP", "ITEMS", "DEBUG", "SYSTEM", "OPTION" };
    const char* tLabels[] = { u8"装備", u8"スキル", u8"地図", u8"所持品", u8"デバッグ", u8"システム", u8"設定" };

    for (int i = 0; i < 7; i++) {
        Rectangle r = { 110.0f + (float)i * 135, 70.0f, 130.0f, 40.0f };
        Color tabColor = (tab == i) ? BLUE : DARKGRAY;
        if (UI::DrawButton(r, tLabels[i], font, tabColor)) { tab = (MenuTab)i; }
    }

    if (tab == EQUIP) {
        DrawTextEx(font, DataManager::uiStrings["ACTIVE_SLOTS"].c_str(), { 120, 130 }, 20, 1, GOLD);
        for (int i = 0; i < 2; i++) {
            int y = 160 + i * 105; bool isEmpty = (p.equippedData[i].id == -1);
            Rectangle slotRect = { 120, (float)y, 260, 95 };
            Rectangle btnRect = { 310, (float)y + 25, 60, 40 };

            Color slotCol = (p.activeSlot == i) ? MAROON : BLACK;
            if (showDetail) slotCol = ColorBrightness(slotCol, -0.4f);
            DrawRectangleRec(slotRect, slotCol);

            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) {
                    if (IsMouseButtonPressed(0)) OpenDetail(p.equippedData[i]);
                }
            }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedData[i]).c_str(), { 130, (float)y + 25 }, 20, 1, WHITE);
                float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[i]);
                DrawTextEx(font, TextFormat("%s +%.1f", DataManager::uiStrings["ATK"].c_str(), totalBonus), { 130, (float)y + 50 }, 14, 1, YELLOW);
                if (UI::DrawButton(btnRect, u8"はずす", font, RED)) p.UnequipWeapon(i);
            }
            else DrawTextEx(font, "EMPTY", { 130, (float)y + 35 }, 20, 1, DARKGRAY);
        }

        const char* armorNames[] = { "HEAD", "CHEST", "HAND", "LEGS", "FEET" };
        for (int i = 0; i < 5; i++) {
            int y = 160 + i * 70;
            DrawTextEx(font, armorNames[i], { 420, (float)y + 20 }, 16, 1, LIGHTGRAY);
            bool isEmpty = (p.equippedArmor[i].id == -1);
            Rectangle slotRect = { 480, (float)y, 200, 60 };
            Rectangle btnRect = { 630, (float)y + 10, 45, 40 };

            DrawRectangleRec(slotRect, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK);
            DrawRectangleLinesEx(slotRect, 1, showDetail ? GRAY : DARKGRAY);

            if (!showDetail && !isEmpty && CheckCollisionPointRec(GetMousePosition(), slotRect)) {
                if (!CheckCollisionPointRec(GetMousePosition(), btnRect)) {
                    if (IsMouseButtonPressed(0)) OpenDetail(p.equippedArmor[i]);
                }
            }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedArmor[i]).c_str(), { 490, (float)y + 10 }, 14, 1, WHITE);
                float def = p.equippedArmor[i].defBonus + DataManager::GetModifier(p.equippedArmor[i].modifierId).def;
                DrawTextEx(font, TextFormat("DEF +%.1f", def), { 490, (float)y + 35 }, 12, 1, BLUE);
                if (UI::DrawButton(btnRect, "OUT", font, RED)) p.UnequipArmor(i);
            }
            else { DrawTextEx(font, "EMPTY", { 490, (float)y + 20 }, 14, 1, DARKGRAY); }
        }

        DrawTextEx(font, DataManager::uiStrings["OWNED_EQUIP"].c_str(), { 720, 130 }, 18, 1, GOLD);
        const int perP = 8; int maxP = (int)ceil((float)p.inventoryEquip.size() / perP); if (maxP < 1) maxP = 1;
        for (int i = 0; i < perP; i++) {
            int idx = equipPage * perP + i; if (idx >= (int)p.inventoryEquip.size()) break;
            int y = 160 + i * 45;
            Rectangle r = { 720, (float)y, 300, 40 };
            DrawRectangleRec(r, showDetail ? ColorBrightness(BLACK, -0.4f) : BLACK);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r)) {
                if (GetMouseX() < 950) {
                    if (IsMouseButtonPressed(0)) OpenDetail(p.inventoryEquip[idx]);
                }
            }
            DrawTextEx(font, Player::GetFullItemName(p.inventoryEquip[idx]).c_str(), { 730, (float)y + 10 }, 14, 1, WHITE);
            if (p.inventoryEquip[idx].type == "EQUIP") {
                if (UI::DrawButton({ 950, (float)y, 30, 40 }, "W1", font, DARKGRAY)) p.EquipWeapon(idx, 0);
                if (UI::DrawButton({ 985, (float)y, 30, 40 }, "W2", font, DARKGRAY)) p.EquipWeapon(idx, 1);
            }
            else if (p.inventoryEquip[idx].type == "ARMOR") {
                int subtype = p.inventoryEquip[idx].weaponSubtype;
                if (subtype >= 0 && subtype < 5) {
                    if (UI::DrawButton({ 950, (float)y, 65, 40 }, "EQUIP", font, DARKGREEN)) p.EquipArmor(idx, subtype);
                }
            }
        }
        if (UI::DrawButton({ 720, 530, 80, 30 }, "<<", font, GRAY) && equipPage > 0) equipPage--;
        if (UI::DrawButton({ 810, 530, 80, 30 }, ">>", font, GRAY) && equipPage < maxP - 1) equipPage++;
    }
    else if (tab == SKILL) {
        Rectangle viewArea = { 100, 120, (float)sw - 200, (float)sh - 170 };
        DrawTextEx(font, u8"右クリックドラッグで視点移動", { 120, 620 }, 16, 1, LIGHTGRAY);
        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT) && !showDetail) { Vector2 delta = GetMouseDelta(); skillOffset = Vector2Add(skillOffset, delta); }
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
        for (auto& node : p.skillTree) { Vector2 startPos = Vector2Add(node.uiPos, skillOffset); for (int reqId : node.reqIds) { Vector2 endPos = Vector2Add(p.skillTree[reqId].uiPos, skillOffset); DrawLineEx(startPos, endPos, 3, node.unlocked ? GOLD : DARKGRAY); } }
        for (int i = 0; i < (int)p.skillTree.size(); i++) { auto& node = p.skillTree[i]; bool available = p.IsSkillAvailable(i); Vector2 drawPos = Vector2Add(node.uiPos, skillOffset); Color nodeColor = node.unlocked ? YELLOW : (available ? GREEN : DARKGRAY); if (node.type != SKILL_PASSIVE) { nodeColor = node.unlocked ? ORANGE : (available ? PURPLE : DARKGRAY); } DrawPoly(drawPos, 6, 35, 0, nodeColor); DrawPolyLines(drawPos, 6, 35, 0, RAYWHITE); DrawTextEx(font, node.name.c_str(), { drawPos.x - 28, drawPos.y - 8 }, 12, 1, node.unlocked ? BLACK : WHITE); if (!node.unlocked) { DrawTextEx(font, TextFormat("SP:%d", node.cost), { drawPos.x - 15, drawPos.y + 15 }, 10, 1, WHITE); } if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea)) { if (available && CheckCollisionPointCircle(GetMousePosition(), drawPos, 35) && IsMouseButtonPressed(0)) { p.UnlockSkill(i); } } }
        EndScissorMode();
    }
    else if (tab == INVENTORY) {
        const char* subK[] = { "CONSUMABLE", "MATERIAL" };
        for (int i = 0; i < 2; i++) {
            Rectangle r = { 120.0f + (float)i * 210, 120, 200, 35 };
            Color c = (itemSubTab == i) ? GREEN : BLACK;
            if (showDetail) c = ColorBrightness(c, -0.4f);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { itemSubTab = i; itemPage = 0; }
            DrawRectangleRec(r, c);
            std::string label = DataManager::uiStrings[subK[i]]; if (label.empty()) label = subK[i];
            DrawTextEx(font, label.c_str(), { r.x + 10, r.y + 8 }, 16, 1, WHITE);
        }
        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL"; for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perP = 10; int maxP = (int)ceil((float)filtered.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(DataManager::uiStrings["PAGE_INFO"].c_str(), itemPage + 1, maxP), { 600, 125 }, 18, 1, WHITE);
        for (int i = 0; i < perP; i++) {
            int lIdx = itemPage * perP + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 165 + i * 42;
            Rectangle itemRect = { 120, (float)y, 400, 38 };
            DrawRectangleRec(itemRect, Fade(BLACK, showDetail ? 0.2f : 0.4f));
            DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), { 135, (float)y + 10 }, 18, 1.0f, WHITE);
            if (!showDetail && CheckCollisionPointRec(GetMousePosition(), itemRect)) {
                if (IsMouseButtonPressed(0)) OpenDetail(item);
            }
            if (itemSubTab == 0 && UI::DrawButton({ 530, (float)y, 80, 38 }, u8"使う", font, GREEN)) p.UseItem(invIdx);
        }
        if (UI::DrawButton({ 120, 600, 100, 30 }, "<<", font, GRAY) && itemPage > 0) itemPage--;
        if (UI::DrawButton({ 230, 600, 100, 30 }, ">>", font, GRAY) && itemPage < maxP - 1) itemPage++;
    }
    else if (tab == DEBUG_TAB) {
        const int perP = 10; int total = (int)DataManager::itemConfigs.size(); int maxP = (int)ceil((float)total / perP);
        for (int i = 0; i < perP; i++) { int idx = debugPage * perP + i; if (idx >= total) break; auto& cfg = DataManager::itemConfigs[idx]; int y = 160 + i * 42; DrawRectangle(120, y, 450, 38, Fade(BLACK, 0.5f)); DrawTextEx(font, TextFormat("[%s] %s", cfg.type.c_str(), cfg.name.c_str()), { 130, (float)y + 10 }, 16, 1, WHITE); if (UI::DrawButton({ 580, (float)y, 60, 38 }, "GET", font, RED)) p.AddToInventory(cfg); }
        if (UI::DrawButton({ 120, 600, 80, 30 }, "<<", font, GRAY) && debugPage > 0) debugPage--; if (UI::DrawButton({ 210, 600, 80, 30 }, ">>", font, GRAY) && debugPage < maxP - 1) debugPage++; if (UI::DrawButton({ 680, 160, 180, 50 }, "SP +999", font, BLUE)) p.skillPoints += 999;
    }
    else if (tab == MAP_TAB) {
        Rectangle viewArea = { 110, 120, (float)sw - 220, (float)sh - 170 };
        DrawTextEx(font, u8"右クリックドラッグでマップ移動", { 120, 620 }, 16, 1, LIGHTGRAY);
        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), viewArea) && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 delta = GetMouseDelta();
            mapOffset = Vector2Add(mapOffset, delta);
        }
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
        float sc = 12.0f;
        float offX = (viewArea.x + viewArea.width / 2.0f) - (d.currentWidth * sc / 2.0f) + mapOffset.x;
        float offY = (viewArea.y + viewArea.height / 2.0f) - (d.currentHeight * sc / 2.0f) + mapOffset.y;
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
        DrawTextEx(font, u8"システムメニュー", { 120, 130 }, 24, 1, WHITE);
        if (d.isHome) {
            if (UI::DrawButton({ 120, 200, 200, 60 }, u8"セーブ", font, BLUE)) { /* Game.cppで処理 */ }
            DrawTextEx(font, u8"現在の状況を保存します", { 340, 220 }, 18, 1, LIGHTGRAY);
        }
        else { DrawRectangle(120, 200, 200, 60, GRAY); DrawTextEx(font, u8"セーブ (ホームのみ)", { 140, 220 }, 18, 1, DARKGRAY); }
        if (UI::DrawButton({ 120, 300, 200, 60 }, u8"タイトルへ戻る", font, RED)) { /* Game.cppで処理 */ }
    }
    else if (tab == OPTION_TAB) {
        DrawTextEx(font, u8"サウンド設定", { 150, 150 }, 24, 1, WHITE);

        DrawTextEx(font, TextFormat("BGM Volume: %.1f", AudioManager::bgmVolume), { 150, 200 }, 20, 1, WHITE);
        Rectangle bgmBar = { 150, 230, 300, 20 };
        DrawRectangleRec(bgmBar, GRAY);
        DrawRectangle(bgmBar.x, bgmBar.y, bgmBar.width * AudioManager::bgmVolume, bgmBar.height, GREEN);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 220, 300, 40 }) && IsMouseButtonDown(0)) {
            float newVol = (GetMouseX() - 150) / 300.0f;
            AudioManager::SetBGMVolume(newVol);
        }

        DrawTextEx(font, TextFormat("SE Volume: %.1f", AudioManager::seVolume), { 150, 300 }, 20, 1, WHITE);
        Rectangle seBar = { 150, 330, 300, 20 };
        DrawRectangleRec(seBar, GRAY);
        DrawRectangle(seBar.x, seBar.y, seBar.width * AudioManager::seVolume, seBar.height, ORANGE);

        if (!showDetail && CheckCollisionPointRec(GetMousePosition(), { 150, 320, 300, 40 }) && IsMouseButtonDown(0)) {
            float newVol = (GetMouseX() - 150) / 300.0f;
            AudioManager::SetSEVolume(newVol);
            if (IsMouseButtonPressed(0)) AudioManager::PlaySE(SE_CLICK);
        }
    }

    DrawDetailWindow(font);
}

void UI::DrawCraftingMenu(Player& p, Font font, bool& isOpen) {
    static bool wasOpen = false;
    static float openTimer = 0.0f;
    if (isOpen && !wasOpen) openTimer = 0.0f;
    wasOpen = isOpen;
    if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);

    int sw = GetScreenWidth(), sh = GetScreenHeight();

    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f));
    DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, ORANGE);

    if (UI::DrawButton({ (float)sw - 160, 60, 100, 40 }, u8"閉じる", font, locked ? Fade(RED, 0.5f) : RED) && !locked) isOpen = false;
    DrawTextEx(font, u8"クラフト", { 80, 70 }, 24, 1, ORANGE);
    DrawTextEx(font, TextFormat("Gold: %d", p.gold), { 250, 75 }, 20, 1, YELLOW);

    const int perPage = 6;
    int maxP = (int)ceil((float)DataManager::recipes.size() / perPage);

    for (int i = 0; i < perPage; i++) {
        int idx = craftingScroll * perPage + i;
        if (idx >= (int)DataManager::recipes.size()) break;

        auto& r = DataManager::recipes[idx];
        ItemData res = DataManager::GetItemConfigCopy(r.resultItemId);

        float y = 120.0f + (float)i * 80.0f;
        Rectangle itemRect = { 80, (float)y, (float)sw - 200, 70 };
        Rectangle createBtn = { (float)sw - 200, y + 15, 80, 40 };

        DrawRectangleRec(itemRect, Fade(DARKGRAY, showDetail ? 0.2f : 0.5f));

        if (!showDetail && !locked && CheckCollisionPointRec(GetMousePosition(), itemRect)) {
            if (!CheckCollisionPointRec(GetMousePosition(), createBtn)) {
                if (IsMouseButtonPressed(0)) OpenDetail(res);
            }
        }

        DrawTextEx(font, res.name.c_str(), { 90, y + 10 }, 20, 1, WHITE);

        std::string matStr = u8"素材: ";
        bool canCraft = true;
        for (auto& m : r.materials) {
            ItemData md = DataManager::GetItemConfigCopy(m.itemId);
            int playerHas = 0;
            for (auto& pi : p.inventoryItems) if (pi.id == m.itemId) playerHas = pi.count;
            if (playerHas < m.count) canCraft = false;
            matStr += TextFormat("%s %d/%d  ", md.name.c_str(), playerHas, m.count);
        }
        DrawTextEx(font, matStr.c_str(), { 90, y + 40 }, 14, 1, LIGHTGRAY);
        DrawTextEx(font, TextFormat(u8"費用: %d G", r.cost), { (float)sw - 320, y + 25 }, 16, 1, (p.gold >= r.cost ? YELLOW : RED));

        if (canCraft && p.gold >= r.cost) {
            if (UI::DrawButton(createBtn, u8"作成", font, locked ? Fade(ORANGE, 0.5f) : ORANGE) && !locked) {
                p.gold -= r.cost;
                for (auto& m : r.materials) {
                    for (auto it = p.inventoryItems.begin(); it != p.inventoryItems.end(); ) {
                        if (it->id == m.itemId) {
                            it->count -= m.count; if (it->count <= 0) it = p.inventoryItems.erase(it); else ++it; break;
                        }
                        else ++it;
                    }
                }
                ItemData newItem = res;
                if (newItem.type == "EQUIP" || newItem.type == "ARMOR") newItem.modifierId = DataManager::GetRandomModifierId();
                p.AddToInventory(newItem);
                AudioManager::PlaySE(SE_REFORGE);
            }
        }
        else { DrawRectangle((int)sw - 200, (int)y + 15, 80, 40, showDetail ? ColorBrightness(GRAY, -0.4f) : GRAY); }
    }

    if (UI::DrawButton({ 80, 620, 100, 30 }, "<<", font, locked ? GRAY : GRAY) && !locked && craftingScroll > 0) craftingScroll--;
    if (UI::DrawButton({ 200, 620, 100, 30 }, ">>", font, locked ? GRAY : GRAY) && !locked && craftingScroll < maxP - 1) craftingScroll++;

    DrawDetailWindow(font);
}

void UI::DrawStorage(Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip) {
    static bool wasOpen = false;
    static float openTimer = 0.0f;
    if (isOpen && !wasOpen) openTimer = 0.0f;
    wasOpen = isOpen;
    if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, GOLD);
    DrawTextEx(font, u8"手持ちアイテム", { 80, 120 }, 20, 1, SKYBLUE);
    const int perP = 10; int mPInv = (int)ceil((float)p.inventoryItems.size() / perP); int mPBox = (int)ceil((float)sItems.size() / perP); if (mPInv < 1)mPInv = 1; if (mPBox < 1)mPBox = 1;
    for (int i = 0; i < perP; i++) {
        int idx = storageInvPage * perP + i; if (idx >= (int)p.inventoryItems.size()) break;
        Rectangle r = { 80, (float)170 + i * 42, 350, 38 };
        DrawRectangleRec(r, showDetail ? ColorBrightness(DARKGRAY, -0.4f) : DARKGRAY);
        DrawTextEx(font, TextFormat("%s x%d", p.inventoryItems[idx].name.c_str(), p.inventoryItems[idx].count), { r.x + 10, r.y + 10 }, 18, 1, WHITE);

        if (!showDetail && !locked && CheckCollisionPointRec(GetMousePosition(), r)) {
            if (IsMouseButtonPressed(0)) OpenDetail(p.inventoryItems[idx]);
        }

        if (UI::DrawButton({ 440, r.y, 110, 38 }, u8"預ける >>", font, locked ? Fade(BLUE, 0.5f) : BLUE) && !locked) {
            bool f = false; for (auto& si : sItems) if (si.id == p.inventoryItems[idx].id) { si.count += p.inventoryItems[idx].count; f = true; break; }
            if (!f) sItems.push_back(p.inventoryItems[idx]); p.inventoryItems.erase(p.inventoryItems.begin() + idx); break;
        }
    }
    if (UI::DrawButton({ 80, 600, 100, 30 }, "<<", font, locked ? GRAY : GRAY) && !locked && storageInvPage > 0) storageInvPage--;
    if (UI::DrawButton({ 190, 600, 100, 30 }, ">>", font, locked ? GRAY : GRAY) && !locked && storageInvPage < mPInv - 1) storageInvPage++;

    DrawTextEx(font, u8"倉庫のアイテム", { 620, 120 }, 20, 1, GREEN);
    for (int i = 0; i < perP; i++) {
        int idx = storageBoxPage * perP + i; if (idx >= (int)sItems.size()) break;
        Rectangle r = { 770, (float)170 + i * 42, 350, 38 };
        DrawRectangleRec(r, showDetail ? ColorBrightness(DARKBLUE, -0.4f) : DARKBLUE);
        DrawTextEx(font, TextFormat("%s x%d", sItems[idx].name.c_str(), sItems[idx].count), { r.x + 10, r.y + 10 }, 18, 1, WHITE);

        if (!showDetail && !locked && CheckCollisionPointRec(GetMousePosition(), r)) {
            if (IsMouseButtonPressed(0)) OpenDetail(sItems[idx]);
        }

        if (UI::DrawButton({ 620, r.y, 110, 38 }, u8"<< 取出す", font, locked ? Fade(DARKGREEN, 0.5f) : DARKGREEN) && !locked) if (p.AddToInventory(sItems[idx])) sItems.erase(sItems.begin() + idx);
    }
    if (UI::DrawButton({ 620, 600, 100, 30 }, "<<", font, locked ? GRAY : GRAY) && !locked && storageBoxPage > 0) storageBoxPage--;
    if (UI::DrawButton({ 730, 600, 100, 30 }, ">>", font, locked ? GRAY : GRAY) && !locked && storageBoxPage < mPBox - 1) storageBoxPage++;
    if (UI::DrawButton({ (float)sw - 160, 70, 100, 45 }, u8"閉じる", font, locked ? Fade(RED, 0.5f) : RED) && !locked) isOpen = false;

    DrawDetailWindow(font);
}

void UI::DrawReforgeMenu(Player& p, Font font, bool& isOpen) {
    static bool wasOpen = false;
    static float openTimer = 0.0f;
    if (isOpen && !wasOpen) openTimer = 0.0f;
    wasOpen = isOpen;
    if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, GOLD);
    if (UI::DrawButton({ (float)sw - 160, 60, 100, 40 }, u8"閉じる", font, locked ? Fade(RED, 0.5f) : RED) && !locked) { isOpen = false; reforgeItemIdx = -1; }
    DrawTextEx(font, u8"リフォージ", { 80, 70 }, 24, 1, GOLD);
    DrawTextEx(font, TextFormat(u8"所持ゴールド: %d G", p.gold), { 80, 110 }, 20, 1, YELLOW);
    const int perPage = 10;
    DrawTextEx(font, u8"リフォージするアイテムを選択", { 80, 150 }, 18, 1, WHITE);
    for (int i = 0; i < (int)p.inventoryEquip.size(); i++) {
        if (i >= perPage) break;
        Rectangle r = { 80, 180.0f + i * 42, 350, 38 };
        Color c = (reforgeItemIdx == i) ? DARKBLUE : DARKGRAY;

        if (DrawButton(r, Player::GetFullItemName(p.inventoryEquip[i]).c_str(), font, locked ? Fade(c, 0.5f) : c) && !locked) { reforgeItemIdx = i; }
    }
    if (reforgeItemIdx != -1 && reforgeItemIdx < (int)p.inventoryEquip.size()) {
        ItemData& item = p.inventoryEquip[reforgeItemIdx];
        DrawRectangle(500, 180, 400, 300, Fade(DARKGRAY, 0.5f));
        DrawTextEx(font, Player::GetFullItemName(item).c_str(), { 520, 200 }, 22, 1, WHITE);
        float baseAtk = item.atkBonus;
        Modifier mod = DataManager::GetModifier(item.modifierId);
        float modAtk = mod.atk;
        float totalAtk = baseAtk + modAtk;
        DrawTextEx(font, TextFormat(u8"基本ATK: %.1f", baseAtk), { 520, 250 }, 18, 1, SKYBLUE);
        DrawTextEx(font, TextFormat(u8"補正ATK: %.1f (%s)", modAtk, mod.name.c_str()), { 520, 280 }, 18, 1, (modAtk >= 0 ? GREEN : RED));
        DrawTextEx(font, TextFormat(u8"合計ATK: %.1f", totalAtk), { 520, 310 }, 20, 1, YELLOW);
        int cost = 50 + p.level * 10 + (int)item.atkBonus * 2;
        DrawTextEx(font, TextFormat(u8"コスト: %d G", cost), { 520, 360 }, 18, 1, GOLD);
        if (p.gold >= cost) {
            if (DrawButton({ 520, 400, 150, 50 }, u8"リフォージ", font, locked ? Fade(GREEN, 0.5f) : GREEN) && !locked) {
                p.gold -= cost;
                item.modifierId = DataManager::GetRandomModifierId();
                AudioManager::PlaySE(SE_REFORGE);
            }
        }
        else {
            DrawRectangle(520, 400, 150, 50, showDetail ? ColorBrightness(GRAY, -0.4f) : GRAY); DrawTextEx(font, u8"ゴールド不足", { 530, 415 }, 18, 1, BLACK);
        }
    }

    DrawDetailWindow(font);
}

int UI::DrawWarpMenu(int maxFloor, Font font, bool& isOpen) {
    static bool wasOpen = false;
    static float openTimer = 0.0f;
    if (isOpen && !wasOpen) openTimer = 0.0f;
    wasOpen = isOpen;
    if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(250, 100, sw - 500, sh - 200, Fade(BLACK, 0.95f)); DrawRectangleLines(250, 100, sw - 500, sh - 200, SKYBLUE);
    DrawTextEx(font, u8"ワープポータル", { 280, 120 }, 24, 1, WHITE);
    if (UI::DrawButton({ (float)sw - 370, 115, 100, 40 }, u8"閉じる", font, locked ? Fade(RED, 0.5f) : RED) && !locked) { isOpen = false; return 0; }
    int selected = 0; std::vector<int> warpFloors; for (int i = 5; i <= maxFloor; i += 5) warpFloors.push_back(i);
    if (warpFloors.empty()) { DrawTextEx(font, u8"ワープ可能な階層がありません", { 300, 200 }, 20, 1, GRAY); }
    else {
        const int perPage = 8; int maxP = (int)ceil((float)warpFloors.size() / perPage);
        for (int i = 0; i < perPage; i++) {
            int idx = warpScroll * perPage + i; if (idx >= (int)warpFloors.size()) break;
            int f = warpFloors[idx]; std::string label = TextFormat(u8"%d 階層", f); if (f % 10 == 0) label += u8" (ボス)"; else if (f % 10 == 5) label += u8" (休憩)";
            float btnX = 280; float btnW = (float)sw - 560.0f; Rectangle r = { btnX, 180.0f + i * 50, btnW, 40 };
            if (UI::DrawButton(r, label.c_str(), font, locked ? Fade(DARKBLUE, 0.5f) : DARKBLUE) && !locked) selected = f;
        }
        float bottomY = sh - 150.0f;
        if (UI::DrawButton({ 300, bottomY, 100, 30 }, "<<", font, locked ? GRAY : GRAY) && !locked && warpScroll > 0) warpScroll--;
        if (UI::DrawButton({ (float)sw - 400, bottomY, 100, 30 }, ">>", font, locked ? GRAY : GRAY) && !locked && warpScroll < maxP - 1) warpScroll++;
    }
    return selected;
}

void UI::DrawLogs(std::vector<GameLog>& logs, Player& p, Camera3D& cam, Font font) {
    Vector3 headPos = Vector3Add(p.position, { 0, 2.0f, 0 }); Vector2 screenPos = GetWorldToScreen(headPos, cam);
    if (screenPos.x < 0 || screenPos.y < 0 || screenPos.x > GetScreenWidth() || screenPos.y > GetScreenHeight()) return;
    for (int i = 0; i < (int)logs.size(); i++) {
        float a = fminf(1.0f, logs[i].life * 2.0f); float moveUp = (4.0f - logs[i].life) * 10.0f; float yOffset = (i * 25.0f) + moveUp;
        Vector2 tSize = MeasureTextEx(font, logs[i].message.c_str(), 20, 1); Vector2 drawPos = { screenPos.x - tSize.x / 2, screenPos.y - 40 - yOffset };
        DrawTextEx(font, logs[i].message.c_str(), { drawPos.x + 1, drawPos.y + 1 }, 20, 1, Fade(BLACK, a)); DrawTextEx(font, logs[i].message.c_str(), drawPos, 20, 1, Fade(logs[i].color, a));
    }
}

void UI::DrawNearbyItems(Player& p, std::vector<DroppedItem>& di, Dungeon& d, Camera3D& cam, Font font) {
    for (auto& item : di) {
        if (!d.IsDiscovered(item.pos.x, item.pos.z)) continue; float d = Vector3Distance(p.position, item.pos);
        if (d < 5.0f) { Vector2 s = GetWorldToScreen(item.pos, cam); if (s.x > 0 && s.y > 0) DrawTextEx(font, Player::GetFullItemName(item.data).c_str(), { s.x - 20, s.y - 20 }, 16, 1, LIME); }
    }
}

int UI::DrawPrompt(const char* label, int sw, int sh, Font font) {
    std::string msg = DataManager::uiStrings[label]; if (msg.empty()) msg = label;
    int bw = 450, bh = 180, bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f)); DrawRectangleLines(bx, by, bw, bh, GOLD);
    Vector2 tS = MeasureTextEx(font, msg.c_str(), 24, 1); DrawTextEx(font, msg.c_str(), { (float)sw / 2 - tS.x / 2, (float)by + 40 }, 24, 1, WHITE);
    Rectangle bY = { (float)sw / 2 - 140, (float)by + 100, 120, 50 }, bN = { (float)sw / 2 + 20, (float)by + 100, 120, 50 };
    int res = 0; if (UI::DrawButton(bY, "YES", font, GREEN)) res = 1; if (UI::DrawButton(bN, "NO", font, RED)) res = 2; return res;
}