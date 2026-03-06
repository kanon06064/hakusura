#include "raylib.h"
#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
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
ItemData UI::detailItem;
double UI::timeOnOpen = 0.0;

int UI::storageSelectedIndex = -1;
bool UI::storageIsDeposit = true;
int UI::storageTransferCount = 1;
int UI::deleteTargetSlot = 0;

// 座標変換 (解像度固定)
Vector2 UI::GetWorldToScreenScaled(Vector3 position, Camera3D camera, int width, int height) {
    Matrix matView = GetCameraMatrix(camera);
    Matrix matProj = MatrixPerspective(camera.fovy * DEG2RAD, (double)width / (double)height, 0.01, 1000.0);
    Matrix matViewProj = MatrixMultiply(matView, matProj);
    Vector3 result = Vector3Transform(position, matViewProj);
    float x = (result.x * 0.5f + 0.5f) * width;
    float y = (1.0f - (result.y * 0.5f + 0.5f)) * height;
    return { x, y };
}

// レイキャスト (解像度固定)
Ray UI::GetMouseRayScaled(Vector2 mousePosition, Camera3D camera, int width, int height) {
    float x = (2.0f * mousePosition.x) / (float)width - 1.0f;
    float y = 1.0f - (2.0f * mousePosition.y) / (float)height;
    Matrix matView = GetCameraMatrix(camera);
    Matrix matProj = MatrixPerspective(camera.fovy * DEG2RAD, (double)width / (double)height, 0.01, 1000.0);
    Matrix matViewProj = MatrixMultiply(matView, matProj);
    Matrix matInv = MatrixInvert(matViewProj);
    Vector3 nearPoint = Vector3Transform({ x, y, -1.0f }, matInv);
    Vector3 farPoint = Vector3Transform({ x, y, 1.0f }, matInv);
    Ray ray;
    ray.position = nearPoint;
    ray.direction = Vector3Normalize(Vector3Subtract(farPoint, nearPoint));
    return ray;
}

bool UI::DrawButton(Rectangle r, const char* label, Font font, Color col, bool active) {
    if (!active) {
        DrawRectangleRec(r, ColorBrightness(col, -0.5f));
        DrawRectangleLinesEx(r, 2, GRAY);
        Vector2 tSize = MeasureTextEx(font, label, 28, 1);
        DrawTextEx(font, label, { r.x + r.width / 2 - tSize.x / 2, r.y + r.height / 2 - tSize.y / 2 }, 28, 1, GRAY);
        return false;
    }
    bool clicked = false; bool hover = CheckCollisionPointRec(GetMousePosition(), r);
    DrawRectangleRec(r, hover ? ColorBrightness(col, 0.2f) : col);
    DrawRectangleLinesEx(r, 2, RAYWHITE);
    Vector2 tSize = MeasureTextEx(font, label, 28, 1);
    DrawTextEx(font, label, { r.x + r.width / 2 - tSize.x / 2, r.y + r.height / 2 - tSize.y / 2 }, 28, 1, WHITE);
    if (hover && IsMouseButtonPressed(0)) clicked = true;
    return clicked;
}

void UI::DrawItemDetail(Font font, int screenW, int screenH) {
    if (!showDetail) return;
    DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.8f));
    float w = 800; float h = 650; float x = (screenW - w) / 2; float y = (screenH - h) / 2;
    DrawRectangle(x, y, w, h, Fade(DARKGRAY, 0.98f)); DrawRectangleLinesEx({ x, y, w, h }, 4, GOLD);

    std::string fullName = Player::GetFullItemName(detailItem);
    DrawTextEx(font, fullName.c_str(), { x + 40, y + 40 }, 60, 1, WHITE);
    DrawTextEx(font, detailItem.type.c_str(), { x + 40, y + 110 }, 32, 1, LIGHTGRAY);

    float statY = y + 180; float lineHeight = 60; int fontSize = 40;
    auto DrawStat = [&](const char* label, float value, bool isPercent = false) {
        if (value != 0) {
            Color valCol = (value > 0) ? GREEN : RED;
            std::string valStr = isPercent ? TextFormat("%s %.0f%%", (value > 0 ? "+" : ""), value * 100) : TextFormat("%s %.1f", (value > 0 ? "+" : ""), value);
            DrawTextEx(font, label, { x + 40, statY }, fontSize, 1, WHITE);
            DrawTextEx(font, valStr.c_str(), { x + 400, statY }, fontSize, 1, valCol);
            statY += lineHeight;
        }
        };

    if (detailItem.type == "EQUIP" || detailItem.type == "ARMOR") {
        Modifier mod = DataManager::GetModifier(detailItem.modifierId);
        DrawStat("攻撃力", detailItem.atkBonus + mod.atk);
        DrawStat("防御力", detailItem.defBonus + mod.def);
        DrawStat("体力補正", detailItem.hpBonus + mod.hp);
        DrawStat("速度補正", detailItem.speedBonus + mod.spd, true);
        if (mod.id != 0) { statY += 30; DrawTextEx(font, TextFormat("接頭辞: %s", mod.name.c_str()), { x + 40, statY }, 36, 1, YELLOW); }
    }
    else if (detailItem.type == "CONSUMABLE") { if (detailItem.heal > 0) DrawStat("回復量", detailItem.heal); }

    if (UI::DrawButton({ x + w / 2 - 100, y + h - 90, 200, 60 }, "閉じる", font, BLUE, true)) { showDetail = false; }

    if (IsMouseButtonPressed(0) && GetTime() > timeOnOpen + 0.2) {
        if (!CheckCollisionPointRec(GetMousePosition(), { x, y, w, h })) { showDetail = false; }
    }
}

int UI::DrawTitleScreen(Font font, int screenW, int screenH) {
    DrawRectangleGradientV(0, 0, screenW, screenH, DARKBLUE, BLACK);
    const char* title = "3D Hack & Slash";
    Vector2 tSize = MeasureTextEx(font, title, 100, 2);
    DrawTextEx(font, title, { (float)(screenW - tSize.x) / 2, 150.0f }, 100, 2, GOLD);

    bool active = (deleteTargetSlot == 0);
    int selectedSlot = 0;

    for (int i = 1; i <= 3; i++) {
        SaveHeader h = DataManager::GetSaveHeader(i);
        float y = 450.0f + (float)(i - 1) * 140.0f;

        Rectangle rMain = { (float)screenW / 2 - 300, y, 500.0f, 100.0f };
        std::string label; Color c;
        if (h.exists) { label = TextFormat("Slot %d: Lv.%d  Floor %d", i, h.playerLevel, h.floor); c = DARKGREEN; }
        else { label = TextFormat("Slot %d: (NO DATA)", i); c = DARKGRAY; }

        if (DrawButton(rMain, label.c_str(), font, c, active)) { selectedSlot = i; }

        if (h.exists) {
            Rectangle rDel = { (float)screenW / 2 + 220, y, 100.0f, 100.0f };
            if (DrawButton(rDel, "削除", font, RED, active)) { deleteTargetSlot = i; }
        }
    }

    if (deleteTargetSlot != 0) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.7f));
        int dw = 600, dh = 300;
        int dx = (screenW - dw) / 2, dy = (screenH - dh) / 2;
        DrawRectangle(dx, dy, dw, dh, DARKGRAY); DrawRectangleLines(dx, dy, dw, dh, RED);

        DrawTextEx(font, "データを削除しますか？", { (float)dx + 50, (float)dy + 60 }, 40, 1, WHITE);
        DrawTextEx(font, "この操作は取り消せません", { (float)dx + 100, (float)dy + 120 }, 24, 1, RED);

        if (DrawButton({ (float)dx + 50, (float)dy + 200, 200, 60 }, "はい", font, RED, true)) {
            DataManager::DeleteSaveData(deleteTargetSlot);
            deleteTargetSlot = 0;
        }
        if (DrawButton({ (float)dx + 350, (float)dy + 200, 200, 60 }, "いいえ", font, BLUE, true)) {
            deleteTargetSlot = 0;
        }
    }
    return selectedSlot;
}

void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, bool debug, Font font, int screenW, int screenH) {
    std::string floorText;
    if (floor == 0) floorText = "HOME"; else { std::string label = "Floor"; if (DataManager::uiStrings.count("FLOOR")) label = DataManager::uiStrings["FLOOR"]; floorText = TextFormat("%s %d", label.c_str(), floor); }
    Vector2 fSize = MeasureTextEx(font, floorText.c_str(), 48, 1);
    DrawRectangle(30, 30, (int)fSize.x + 40, 60, Fade(BLACK, 0.6f));
    DrawTextEx(font, floorText.c_str(), { 50, 36 }, 48, 1, WHITE);

    // 敵HPバーリスト (頭上表示とは別にリスト表示も残す場合)
    int listCount = 0;
    for (auto& e : enemies) {
        if (e.hudTimer > 0) {
            int yPos = 100 + listCount * 70;
            DrawRectangle(screenW - 350, yPos, 330, 60, Fade(BLACK, 0.7f));
            DrawTextEx(font, TextFormat("Lv.%d %s", e.level, e.data.name.c_str()), { (float)screenW - 330, (float)yPos + 5 }, 28, 1, WHITE);
            DrawRectangle(screenW - 330, yPos + 40, 290, 14, DARKGRAY);
            DrawRectangle(screenW - 330, yPos + 40, (int)(290.0f * (e.hp / e.maxHp)), 14, RED);
            listCount++; if (listCount >= 5) break;
        }
    }

    DrawRectangle(20, screenH - 220, 500, 200, Fade(BLACK, 0.6f));
    DrawTextEx(font, TextFormat("Lv: %d   EXP: %d/%d", p.level, p.exp, p.expToNext), { 40, (float)screenH - 200 }, 36, 1, SKYBLUE);
    DrawRectangle(40, screenH - 150, 460, 30, DARKGRAY);
    DrawRectangle(40, screenH - 150, (int)(460 * (fmaxf(0.0f, p.hp) / p.maxHp)), 30, GREEN);
    DrawTextEx(font, TextFormat("HP: %.0f/%.0f", p.hp, p.maxHp), { 50, (float)screenH - 152 }, 24, 1, WHITE);
    DrawTextEx(font, TextFormat("ATK: %.1f  DEF: %.1f", p.attackPower, p.defense), { 40, (float)screenH - 100 }, 32, 1, WHITE);
    DrawTextEx(font, TextFormat("Gold: %d  SP: %d", p.gold, p.skillPoints), { 40, (float)screenH - 60 }, 32, 1, WHITE);

    int iconSize = 100; int startX = screenW - 350; int startY = screenH - 130;
    struct SkillIcon { SkillType type; const char* label; const char* key; };
    SkillIcon icons[] = { { SKILL_ACTIVE_DASH, "DASH", "1" }, { SKILL_ACTIVE_SMASH, "SMASH", "2" }, { SKILL_ACTIVE_STEALTH, "STEALTH", "3" } };
    for (int i = 0; i < 3; i++) {
        int x = startX + i * (iconSize + 15); bool unlocked = p.IsSkillUnlocked(icons[i].type); Color baseCol = unlocked ? DARKBLUE : DARKGRAY;
        DrawRectangle(x, startY, iconSize, iconSize, baseCol); DrawRectangleLines(x, startY, iconSize, iconSize, RAYWHITE); DrawTextEx(font, icons[i].key, { (float)x + 5, (float)startY + 5 }, 24, 1, WHITE);
        if (unlocked) {
            float cd = p.GetSkillCooldown(icons[i].type); float maxCd = p.GetSkillMaxCooldown(icons[i].type);
            if (cd > 0) { float ratio = cd / maxCd; DrawRectangle(x, startY + (int)((float)iconSize * (1.0f - ratio)), iconSize, (int)((float)iconSize * ratio), Fade(RED, 0.7f)); DrawTextEx(font, TextFormat("%.1f", cd), { (float)x + 15, (float)startY + 30 }, 30, 1, YELLOW); }
            else { std::string label = icons[i].label; if (DataManager::uiStrings.count(label)) label = DataManager::uiStrings[label]; DrawTextEx(font, label.c_str(), { (float)x + 5, (float)startY + 40 }, 20, 1, GREEN); }
        }
        else { DrawTextEx(font, "LOCK", { (float)x + 15, (float)startY + 40 }, 20, 1, GRAY); }
    }
}

void UI::DrawMenu(Player& p, Dungeon& d, MenuTab& tab, Font font, int screenW, int screenH, bool inputEnabled) {
    bool active = inputEnabled && !showDetail;
    DrawRectangle(100, 50, screenW - 200, screenH - 100, Fade(DARKGRAY, 0.95f));
    const char* tKeys[] = { "EQUIP", "SKILL", "MAP", "ITEMS", "DEBUG", "SYSTEM" };
    for (int i = 0; i < 6; i++) {
        Rectangle r = { 110.0f + (float)i * 160, 60.0f, 150.0f, 60.0f };
        if (active && CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { tab = (MenuTab)i; }
        DrawRectangleRec(r, (tab == i) ? BLUE : DARKGRAY);
        std::string label = DataManager::uiStrings[tKeys[i]]; if (label.empty()) label = tKeys[i];
        Vector2 lSize = MeasureTextEx(font, label.c_str(), 30, 1);
        DrawTextEx(font, label.c_str(), { r.x + (r.width / 2 - lSize.x / 2), r.y + 15 }, 30, 1, WHITE);
    }

    if (tab == EQUIP) {
        DrawTextEx(font, DataManager::uiStrings["ACTIVE_SLOTS"].c_str(), { 150, 150 }, 32, 1, GOLD);
        for (int i = 0; i < 2; i++) {
            int y = 200 + i * 160; bool isEmpty = (p.equippedData[i].id == -1);
            DrawRectangle(150, y, 400, 140, (p.activeSlot == i) ? MAROON : BLACK);
            Rectangle detailsRect = { 150, (float)y, 250, 140 };
            if (active && !isEmpty && CheckCollisionPointRec(GetMousePosition(), detailsRect) && IsMouseButtonPressed(0)) { showDetail = true; detailItem = p.equippedData[i]; timeOnOpen = GetTime(); }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedData[i]).c_str(), { 160, (float)y + 30 }, 36, 1, WHITE);
                float totalBonus = Player::GetItemTotalAtkBonus(p.equippedData[i]);
                DrawTextEx(font, TextFormat("%s +%.1f", DataManager::uiStrings["ATK"].c_str(), totalBonus), { 160, (float)y + 80 }, 30, 1, YELLOW);
                if (UI::DrawButton({ 420, (float)y + 40, 100, 60 }, "はずす", font, RED, active)) p.UnequipWeapon(i);
            }
            else DrawTextEx(font, "EMPTY", { 160, (float)y + 50 }, 36, 1, DARKGRAY);
        }
        const char* armorNames[] = { "HEAD", "CHEST", "HAND", "LEGS", "FEET" };
        for (int i = 0; i < 5; i++) {
            int y = 200 + i * 110; DrawTextEx(font, armorNames[i], { 580, (float)y + 30 }, 24, 1, LIGHTGRAY);
            bool isEmpty = (p.equippedArmor[i].id == -1); DrawRectangle(680, y, 300, 90, BLACK); DrawRectangleLines(680, y, 300, 90, DARKGRAY);
            Rectangle detailsRect = { 680, (float)y, 210, 90 };
            if (active && !isEmpty && CheckCollisionPointRec(GetMousePosition(), detailsRect) && IsMouseButtonPressed(0)) { showDetail = true; detailItem = p.equippedArmor[i]; timeOnOpen = GetTime(); }
            if (!isEmpty) {
                DrawTextEx(font, Player::GetFullItemName(p.equippedArmor[i]).c_str(), { 690, (float)y + 20 }, 28, 1, WHITE);
                float def = p.equippedArmor[i].defBonus + DataManager::GetModifier(p.equippedArmor[i].modifierId).def;
                DrawTextEx(font, TextFormat("DEF +%.1f", def), { 690, (float)y + 60 }, 24, 1, BLUE);
                if (UI::DrawButton({ 890, (float)y + 25, 80, 40 }, "OUT", font, RED, active)) p.UnequipArmor(i);
            }
            else { DrawTextEx(font, "EMPTY", { 690, (float)y + 35 }, 28, 1, DARKGRAY); }
        }
        DrawTextEx(font, DataManager::uiStrings["OWNED_EQUIP"].c_str(), { 1050, 150 }, 32, 1, GOLD);
        const int perP = 7; int maxP = (int)ceil((float)p.inventoryEquip.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(DataManager::uiStrings["PAGE_INFO"].c_str(), equipPage + 1, maxP), { 1400, 155 }, 28, 1, WHITE);
        for (int i = 0; i < perP; i++) {
            int idx = equipPage * perP + i; if (idx >= (int)p.inventoryEquip.size()) break;
            int y = 200 + i * 75; Rectangle r = { 1050, (float)y, 450, 65 }; DrawRectangleRec(r, BLACK);
            Rectangle detailsRect = { 1050, (float)y, 330, 65 };
            if (active && CheckCollisionPointRec(GetMousePosition(), detailsRect) && IsMouseButtonPressed(0)) { showDetail = true; detailItem = p.inventoryEquip[idx]; timeOnOpen = GetTime(); }
            DrawTextEx(font, Player::GetFullItemName(p.inventoryEquip[idx]).c_str(), { 1060, (float)y + 20 }, 30, 1, WHITE);
            if (p.inventoryEquip[idx].type == "EQUIP") { if (UI::DrawButton({ 1380, (float)y + 10, 50, 45 }, "W1", font, DARKGRAY, active)) p.EquipWeapon(idx, 0); if (UI::DrawButton({ 1440, (float)y + 10, 50, 45 }, "W2", font, DARKGRAY, active)) p.EquipWeapon(idx, 1); }
            else if (p.inventoryEquip[idx].type == "ARMOR") { int subtype = p.inventoryEquip[idx].weaponSubtype; if (subtype >= 0 && subtype < 5) { if (UI::DrawButton({ 1380, (float)y + 10, 110, 45 }, "EQUIP", font, DARKGREEN, active)) p.EquipArmor(idx, subtype); } }
        }
        if (UI::DrawButton({ 1050, 750, 120, 50 }, "<<", font, GRAY, active) && equipPage > 0) equipPage--; if (UI::DrawButton({ 1200, 750, 120, 50 }, ">>", font, GRAY, active)) equipPage++;
    }
    else if (tab == SKILL) {
        Rectangle viewArea = { 100, 150, (float)screenW - 200, (float)screenH - 250 };
        DrawTextEx(font, "右クリックドラッグで視点移動", { 150, (float)screenH - 80 }, 24, 1, LIGHTGRAY);
        if (active && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) { Vector2 delta = GetMouseDelta(); skillOffset = Vector2Add(skillOffset, delta); }
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
        for (auto& node : p.skillTree) { Vector2 startPos = Vector2Add(node.uiPos, skillOffset); for (int reqId : node.reqIds) { Vector2 endPos = Vector2Add(p.skillTree[reqId].uiPos, skillOffset); DrawLineEx(startPos, endPos, 4, node.unlocked ? GOLD : DARKGRAY); } }
        for (int i = 0; i < (int)p.skillTree.size(); i++) {
            auto& node = p.skillTree[i]; bool available = p.IsSkillAvailable(i); Vector2 drawPos = Vector2Add(node.uiPos, skillOffset); Color nodeColor = node.unlocked ? YELLOW : (available ? GREEN : DARKGRAY); if (node.type != SKILL_PASSIVE) { nodeColor = node.unlocked ? ORANGE : (available ? PURPLE : DARKGRAY); } DrawPoly(drawPos, 6, 55, 0, nodeColor); DrawPolyLines(drawPos, 6, 55, 0, RAYWHITE); DrawTextEx(font, node.name.c_str(), { drawPos.x - 40, drawPos.y - 12 }, 16, 1, node.unlocked ? BLACK : WHITE); if (!node.unlocked) { DrawTextEx(font, TextFormat("SP:%d", node.cost), { drawPos.x - 25, drawPos.y + 25 }, 16, 1, WHITE); }
            if (active && CheckCollisionPointRec(GetMousePosition(), viewArea)) { if (available && CheckCollisionPointCircle(GetMousePosition(), drawPos, 55) && IsMouseButtonPressed(0)) { p.UnlockSkill(i); } }
        }
        EndScissorMode();
    }
    else if (tab == INVENTORY) {
        const char* subK[] = { "CONSUMABLE", "MATERIAL" };
        for (int i = 0; i < 2; i++) { Rectangle r = { 150.0f + (float)i * 300, 150, 280, 60 }; if (active && CheckCollisionPointRec(GetMousePosition(), r) && IsMouseButtonPressed(0)) { itemSubTab = i; itemPage = 0; } DrawRectangleRec(r, (itemSubTab == i) ? GREEN : BLACK); std::string label = DataManager::uiStrings[subK[i]]; if (label.empty()) label = subK[i]; DrawTextEx(font, label.c_str(), { r.x + 30, r.y + 15 }, 28, 1, WHITE); }
        std::vector<int> filtered; std::string target = (itemSubTab == 0) ? "CONSUMABLE" : "MATERIAL"; for (int i = 0; i < (int)p.inventoryItems.size(); i++) if (p.inventoryItems[i].type == target) filtered.push_back(i);
        const int perP = 8; int maxP = (int)ceil((float)filtered.size() / perP); if (maxP < 1) maxP = 1;
        DrawTextEx(font, TextFormat(DataManager::uiStrings["PAGE_INFO"].c_str(), itemPage + 1, maxP), { 800, 160 }, 28, 1, WHITE);
        for (int i = 0; i < perP; i++) {
            int lIdx = itemPage * perP + i; if (lIdx >= (int)filtered.size()) break;
            int invIdx = filtered[lIdx]; auto& item = p.inventoryItems[invIdx];
            int y = 230 + i * 65; Rectangle r = { 150, (float)y, 700, 60 }; DrawRectangleRec(r, Fade(BLACK, 0.4f));
            Rectangle detailsRect = { 150, (float)y, 550, 60 };
            if (active && CheckCollisionPointRec(GetMousePosition(), detailsRect) && IsMouseButtonPressed(0)) { showDetail = true; detailItem = item; timeOnOpen = GetTime(); }
            DrawTextEx(font, TextFormat("%s x%d", item.name.c_str(), item.count), { 170, (float)y + 15 }, 32, 1.0f, WHITE);
            if (itemSubTab == 0 && UI::DrawButton({ 700, (float)y + 5, 120, 50 }, "使う", font, GREEN, active)) p.UseItem(invIdx);
        }
        if (UI::DrawButton({ 150, 800, 140, 50 }, "<<", font, GRAY, active)) itemPage--; if (UI::DrawButton({ 350, 800, 140, 50 }, ">>", font, GRAY, active)) itemPage++;
    }
    else if (tab == DEBUG_TAB) {
        const int perP = 6; int total = (int)DataManager::itemConfigs.size(); int maxP = (int)ceil((float)total / perP);
        DrawTextEx(font, TextFormat("PAGE %d/%d", debugPage + 1, maxP), { 800, 160 }, 28, 1, WHITE);
        for (int i = 0; i < perP; i++) { int idx = debugPage * perP + i; if (idx >= total) break; auto& cfg = DataManager::itemConfigs[idx]; int y = 220 + i * 80; DrawRectangle(120, y, 700, 70, Fade(BLACK, 0.5f)); DrawTextEx(font, TextFormat("[%s] %s", cfg.type.c_str(), cfg.name.c_str()), { 130, (float)y + 20 }, 30, 1, WHITE); if (UI::DrawButton({ 700, (float)y + 10, 100, 50 }, "GET", font, RED, active)) p.AddToInventory(cfg); }
        if (UI::DrawButton({ 120, 800, 120, 50 }, "<<", font, GRAY, active) && debugPage > 0) debugPage--; if (UI::DrawButton({ 260, 800, 120, 50 }, ">>", font, GRAY, active) && debugPage < maxP - 1) debugPage++; if (UI::DrawButton({ 900, 160, 200, 60 }, "SP +999", font, BLUE, active)) p.skillPoints += 999;
    }
    else if (tab == MAP_TAB) {
        float sc = 15.0f; float offX = (screenW / 2.0f) - (d.currentWidth * sc / 2.0f) + mapOffset.x; float offY = (screenH / 2.0f) - (d.currentHeight * sc / 2.0f) + mapOffset.y;
        if (active && IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) { Vector2 delta = GetMouseDelta(); mapOffset = Vector2Add(mapOffset, delta); }
        Rectangle viewArea = { 100, 150, (float)screenW - 200, (float)screenH - 250 };
        BeginScissorMode((int)viewArea.x, (int)viewArea.y, (int)viewArea.width, (int)viewArea.height);
        DrawRectangle((int)offX - 10, (int)offY - 10, (int)(d.currentWidth * sc) + 20, (int)(d.currentHeight * sc) + 20, BLACK);
        for (int y = 0; y < d.currentHeight; y++) for (int x = 0; x < d.currentWidth; x++) if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) DrawRectangle((int)(offX + x * sc), (int)(offY + y * sc), (int)sc - 1, (int)sc - 1, d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN);
        DrawCircle((int)(offX + (p.position.x / TILE_SIZE) * sc), (int)(offY + (p.position.z / TILE_SIZE) * sc), 8, RED);
        EndScissorMode();
        DrawTextEx(font, "右クリックドラッグで地図移動", { 150, (float)screenH - 80 }, 24, 1, LIGHTGRAY);
    }
    else if (tab == SYSTEM_TAB) {
        DrawTextEx(font, "システムメニュー", { 150, 150 }, 36, 1, WHITE);
        if (d.isHome) { if (UI::DrawButton({ 150, 250, 300, 80 }, "セーブ", font, BLUE, active)) { /* Game.cppで処理 */ } DrawTextEx(font, "現在の状況を保存します", { 470, 275 }, 28, 1, LIGHTGRAY); }
        else { DrawRectangle(150, 250, 300, 80, GRAY); DrawTextEx(font, "セーブ (ホームのみ)", { 170, 275 }, 28, 1, DARKGRAY); }
        if (UI::DrawButton({ 150, 400, 300, 80 }, "タイトルへ戻る", font, RED, active)) { /* Game.cppで処理 */ }
    }
}

// DrawCraftingMenu は変更なしのため省略せず記述
void UI::DrawCraftingMenu(Player& p, Font font, bool& isOpen, int screenW, int screenH, bool inputEnabled) {
    bool active = inputEnabled && !showDetail;
    DrawRectangle(100, 100, screenW - 200, screenH - 200, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 100, 100, (float)screenW - 200, (float)screenH - 200 }, 3, ORANGE);
    if (UI::DrawButton({ (float)screenW - 250, 120, 120, 50 }, "閉じる", font, RED, active)) isOpen = false;
    DrawTextEx(font, "クラフト", { 150, 130 }, 36, 1, ORANGE);
    DrawTextEx(font, TextFormat("Gold: %d", p.gold), { 350, 135 }, 28, 1, YELLOW);
    const int perPage = 6; int maxP = (int)ceil((float)DataManager::recipes.size() / perPage);
    for (int i = 0; i < perPage; i++) {
        int idx = craftingScroll * perPage + i; if (idx >= (int)DataManager::recipes.size()) break;
        auto& r = DataManager::recipes[idx];
        ItemData res = DataManager::GetItemConfigCopy(r.resultItemId);
        float y = 200.0f + (float)i * 110.0f;
        Rectangle boxRect = { 150, y, (float)screenW - 300, 90 };
        DrawRectangleRec(boxRect, Fade(DARKGRAY, 0.5f));
        if (active && CheckCollisionPointRec(GetMousePosition(), boxRect) && IsMouseButtonPressed(0)) { showDetail = true; detailItem = res; timeOnOpen = GetTime(); }
        DrawTextEx(font, res.name.c_str(), { 160, y + 15 }, 32, 1, WHITE);
        std::string matStr = "素材: ";
        bool canCraft = true;
        for (auto& m : r.materials) {
            ItemData md = DataManager::GetItemConfigCopy(m.itemId);
            int playerHas = 0; for (auto& pi : p.inventoryItems) if (pi.id == m.itemId) playerHas = pi.count;
            if (playerHas < m.count) canCraft = false;
            matStr += TextFormat("%s %d/%d  ", md.name.c_str(), playerHas, m.count);
        }
        DrawTextEx(font, matStr.c_str(), { 160, y + 55 }, 24, 1, LIGHTGRAY);
        DrawTextEx(font, TextFormat("費用: %d G", r.cost), { (float)screenW - 450, y + 35 }, 28, 1, (p.gold >= r.cost ? YELLOW : RED));
        if (canCraft && p.gold >= r.cost) {
            if (UI::DrawButton({ (float)screenW - 300, y + 20, 100, 50 }, "作成", font, ORANGE, active)) {
                p.gold -= r.cost;
                for (auto& m : r.materials) {
                    for (auto it = p.inventoryItems.begin(); it != p.inventoryItems.end(); ) { if (it->id == m.itemId) { it->count -= m.count; if (it->count <= 0) it = p.inventoryItems.erase(it); else ++it; break; } else ++it; }
                }
                ItemData newItem = res;
                if (newItem.type == "EQUIP" || newItem.type == "ARMOR") newItem.modifierId = DataManager::GetRandomModifierId();
                p.AddToInventory(newItem);
            }
        }
        else { DrawRectangle((int)screenW - 300, (int)y + 20, 100, 50, GRAY); DrawTextEx(font, "作成", { (float)screenW - 275, y + 30 }, 28, 1, DARKGRAY); }
    }
    if (UI::DrawButton({ 150, 850, 140, 50 }, "<<", font, GRAY, active) && craftingScroll > 0) craftingScroll--;
    if (UI::DrawButton({ 350, 850, 140, 50 }, ">>", font, GRAY, active) && craftingScroll < maxP - 1) craftingScroll++;
}

void UI::DrawStorage(Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip, int screenW, int screenH, bool inputEnabled) {
    bool active = inputEnabled && !showDetail && (storageSelectedIndex == -1);
    DrawRectangle(50, 50, screenW - 100, screenH - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)screenW - 100, (float)screenH - 100 }, 3, GOLD);
    DrawTextEx(font, "手持ちアイテム", { 100, 120 }, 32, 1, SKYBLUE);
    const int perP = 8; int mPInv = (int)ceil((float)p.inventoryItems.size() / perP); int mPBox = (int)ceil((float)sItems.size() / perP); if (mPInv < 1)mPInv = 1; if (mPBox < 1)mPBox = 1;
    for (int i = 0; i < perP; i++) {
        int idx = storageInvPage * perP + i; if (idx >= (int)p.inventoryItems.size()) break;
        Rectangle r = { 100, (float)180 + i * 65, 600, 60 }; DrawRectangleRec(r, DARKGRAY);
        if (active && CheckCollisionPointRec(GetMousePosition(), { 100, (float)180 + i * 65, 450, 60 }) && IsMouseButtonPressed(0)) { showDetail = true; detailItem = p.inventoryItems[idx]; timeOnOpen = GetTime(); }
        DrawTextEx(font, TextFormat("%s x%d", p.inventoryItems[idx].name.c_str(), p.inventoryItems[idx].count), { r.x + 15, r.y + 15 }, 30, 1, WHITE);
        if (UI::DrawButton({ 550, r.y + 5, 140, 50 }, "預ける >>", font, BLUE, active)) { storageSelectedIndex = idx; storageIsDeposit = true; storageTransferCount = 1; }
    }
    if (UI::DrawButton({ 100, 850, 120, 50 }, "<<", font, GRAY, active) && storageInvPage > 0) storageInvPage--;
    if (UI::DrawButton({ 250, 850, 120, 50 }, ">>", font, GRAY, active) && storageInvPage < mPInv - 1) storageInvPage++;

    DrawTextEx(font, "倉庫のアイテム", { 900, 120 }, 32, 1, GREEN);
    for (int i = 0; i < perP; i++) {
        int idx = storageBoxPage * perP + i; if (idx >= (int)sItems.size()) break;
        Rectangle r = { 1100, (float)180 + i * 65, 600, 60 }; DrawRectangleRec(r, DARKBLUE);
        if (active && CheckCollisionPointRec(GetMousePosition(), { 1100, (float)180 + i * 65, 450, 60 }) && IsMouseButtonPressed(0)) { showDetail = true; detailItem = sItems[idx]; timeOnOpen = GetTime(); }
        DrawTextEx(font, TextFormat("%s x%d", sItems[idx].name.c_str(), sItems[idx].count), { r.x + 15, r.y + 15 }, 30, 1, WHITE);
        if (UI::DrawButton({ 900, r.y + 5, 140, 50 }, "<< 取出す", font, DARKGREEN, active)) { storageSelectedIndex = idx; storageIsDeposit = false; storageTransferCount = 1; }
    }
    if (UI::DrawButton({ 900, 850, 120, 50 }, "<<", font, GRAY, active) && storageBoxPage > 0) storageBoxPage--;
    if (UI::DrawButton({ 1050, 850, 120, 50 }, ">>", font, GRAY, active) && storageBoxPage < mPBox - 1) storageBoxPage++;
    if (UI::DrawButton({ (float)screenW - 200, 80, 140, 60 }, "閉じる", font, RED, active)) isOpen = false;

    if (storageSelectedIndex != -1) {
        DrawRectangle(0, 0, screenW, screenH, Fade(BLACK, 0.5f));
        int mw = 500, mh = 300; int mx = screenW / 2 - mw / 2, my = screenH / 2 - mh / 2;
        DrawRectangle(mx, my, mw, mh, DARKGRAY); DrawRectangleLines(mx, my, mw, mh, RAYWHITE);
        ItemData* targetItem = nullptr; int maxCount = 0;
        if (storageIsDeposit) { if (storageSelectedIndex < (int)p.inventoryItems.size()) { targetItem = &p.inventoryItems[storageSelectedIndex]; maxCount = targetItem->count; } }
        else { if (storageSelectedIndex < (int)sItems.size()) { targetItem = &sItems[storageSelectedIndex]; maxCount = targetItem->count; } }

        if (targetItem) {
            DrawTextEx(font, TextFormat("%s を%s", targetItem->name.c_str(), storageIsDeposit ? "預ける" : "取り出す"), { (float)mx + 30, (float)my + 30 }, 28, 1, WHITE);
            if (UI::DrawButton({ (float)mx + 50, (float)my + 100, 50, 50 }, "-1", font, RED, true)) storageTransferCount--;
            if (UI::DrawButton({ (float)mx + 110, (float)my + 100, 60, 50 }, "-10", font, RED, true)) storageTransferCount -= 10;
            DrawTextEx(font, TextFormat("%d", storageTransferCount), { (float)mx + 220, (float)my + 110 }, 36, 1, WHITE);
            if (UI::DrawButton({ (float)mx + 300, (float)my + 100, 50, 50 }, "+1", font, GREEN, true)) storageTransferCount++;
            if (UI::DrawButton({ (float)mx + 360, (float)my + 100, 60, 50 }, "+10", font, GREEN, true)) storageTransferCount += 10;
            if (storageTransferCount < 1) storageTransferCount = 1; if (storageTransferCount > maxCount) storageTransferCount = maxCount;
            if (UI::DrawButton({ (float)mx + 50, (float)my + 200, 150, 50 }, "決定", font, BLUE, true)) {
                if (storageIsDeposit) {
                    ItemData moveItem = *targetItem; moveItem.count = storageTransferCount;
                    bool added = false; for (auto& si : sItems) if (si.id == moveItem.id) { si.count += moveItem.count; added = true; break; }
                    if (!added) sItems.push_back(moveItem);
                    targetItem->count -= storageTransferCount; if (targetItem->count <= 0) p.inventoryItems.erase(p.inventoryItems.begin() + storageSelectedIndex);
                }
                else {
                    ItemData moveItem = *targetItem; moveItem.count = storageTransferCount; p.AddToInventory(moveItem);
                    targetItem->count -= storageTransferCount; if (targetItem->count <= 0) sItems.erase(sItems.begin() + storageSelectedIndex);
                }
                storageSelectedIndex = -1;
            }
            if (UI::DrawButton({ (float)mx + 280, (float)my + 200, 150, 50 }, "キャンセル", font, DARKGRAY, true)) { storageSelectedIndex = -1; }
        }
        else { storageSelectedIndex = -1; }
    }
    else {
        if (UI::DrawButton({ (float)screenW - 200, 80, 140, 60 }, "閉じる", font, RED, active)) isOpen = false;
    }
}

void UI::DrawReforgeMenu(Player& p, Font font, bool& isOpen, int screenW, int screenH, bool inputEnabled) {
    bool active = inputEnabled && !showDetail;
    DrawRectangle(50, 50, screenW - 100, screenH - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)screenW - 100, (float)screenH - 100 }, 3, GOLD);
    if (UI::DrawButton({ (float)screenW - 200, 80, 140, 60 }, "閉じる", font, RED, active)) { isOpen = false; reforgeItemIdx = -1; }
    DrawTextEx(font, "リフォージ", { 100, 100 }, 36, 1, GOLD);
    DrawTextEx(font, TextFormat("所持ゴールド: %d G", p.gold), { 100, 150 }, 28, 1, YELLOW);
    const int perPage = 10;
    DrawTextEx(font, "リフォージするアイテムを選択", { 100, 200 }, 24, 1, WHITE);
    for (int i = 0; i < (int)p.inventoryEquip.size(); i++) {
        if (i >= perPage) break;
        Rectangle r = { 100, 240.0f + i * 65, 500, 60 };
        Color c = (reforgeItemIdx == i) ? DARKBLUE : DARKGRAY;
        if (DrawButton(r, Player::GetFullItemName(p.inventoryEquip[i]).c_str(), font, c, active)) { reforgeItemIdx = i; }
    }
    if (reforgeItemIdx != -1 && reforgeItemIdx < (int)p.inventoryEquip.size()) {
        ItemData& item = p.inventoryEquip[reforgeItemIdx];
        DrawRectangle(700, 240, 600, 400, Fade(DARKGRAY, 0.5f));
        DrawTextEx(font, Player::GetFullItemName(item).c_str(), { 720, 260 }, 32, 1, WHITE);
        float baseAtk = item.atkBonus;
        Modifier mod = DataManager::GetModifier(item.modifierId);
        float modAtk = mod.atk;
        float totalAtk = baseAtk + modAtk;
        DrawTextEx(font, TextFormat("基本ATK: %.1f", baseAtk), { 720, 320 }, 24, 1, SKYBLUE);
        DrawTextEx(font, TextFormat("補正ATK: %.1f (%s)", modAtk, mod.name.c_str()), { 720, 360 }, 24, 1, (modAtk >= 0 ? GREEN : RED));
        DrawTextEx(font, TextFormat("合計ATK: %.1f", totalAtk), { 720, 400 }, 28, 1, YELLOW);
        int cost = 50 + p.level * 10 + (int)item.atkBonus * 2;
        DrawTextEx(font, TextFormat("コスト: %d G", cost), { 720, 460 }, 24, 1, GOLD);
        if (p.gold >= cost) {
            if (DrawButton({ 720, 520, 200, 60 }, "リフォージ", font, GREEN, active)) { p.gold -= cost; item.modifierId = DataManager::GetRandomModifierId(); }
        }
        else {
            DrawRectangle(720, 520, 200, 60, GRAY); DrawTextEx(font, "ゴールド不足", { 730, 535 }, 24, 1, BLACK);
        }
    }
}

int UI::DrawWarpMenu(int maxFloor, Font font, bool& isOpen, bool inputEnabled, int screenW, int screenH) {
    DrawRectangle(300, 150, screenW - 600, screenH - 300, Fade(BLACK, 0.95f)); DrawRectangleLines(300, 150, screenW - 600, screenH - 300, SKYBLUE);
    DrawTextEx(font, "ワープポータル", { 350, 180 }, 36, 1, WHITE);
    if (inputEnabled && UI::DrawButton({ (float)screenW - 500, 170, 140, 50 }, "閉じる", font, RED)) { isOpen = false; return 0; }
    else if (!inputEnabled) { DrawRectangleRec({ (float)screenW - 500, 170, 140, 50 }, GRAY); DrawTextEx(font, "閉じる", { (float)screenW - 480, 185 }, 24, 1, WHITE); }

    int selected = 0; std::vector<int> warpFloors; for (int i = 5; i <= maxFloor; i += 5) warpFloors.push_back(i);
    if (warpFloors.empty()) { DrawTextEx(font, "ワープ可能な階層がありません", { 400, 300 }, 28, 1, GRAY); }
    else {
        const int perPage = 6; int maxP = (int)ceil((float)warpFloors.size() / perPage);
        for (int i = 0; i < perPage; i++) {
            int idx = warpScroll * perPage + i; if (idx >= (int)warpFloors.size()) break;
            int f = warpFloors[idx]; std::string label = TextFormat("%d 階層", f); if (f % 10 == 0) label += " (ボス)"; else if (f % 10 == 5) label += " (休憩)";
            float btnX = 350; float btnW = (float)screenW - 700.0f; Rectangle r = { btnX, 260.0f + i * 80, btnW, 60 };
            if (inputEnabled) { if (UI::DrawButton(r, label.c_str(), font, DARKBLUE)) selected = f; }
            else { DrawRectangleRec(r, DARKGRAY); DrawTextEx(font, label.c_str(), { r.x + 20, r.y + 15 }, 24, 1, WHITE); }
        }
        float bottomY = (float)screenH - 220.0f;
        if (inputEnabled) {
            if (UI::DrawButton({ 400, bottomY, 140, 50 }, "<<", font, GRAY) && warpScroll > 0) warpScroll--;
            if (UI::DrawButton({ (float)screenW - 550, bottomY, 140, 50 }, ">>", font, GRAY) && warpScroll < maxP - 1) warpScroll++;
        }
    }
    return selected;
}

void UI::DrawLogs(std::vector<GameLog>& logs, Player& p, Camera3D& cam, Font font, int screenW, int screenH) {
    Vector3 headPos = Vector3Add(p.position, { 0, 2.0f, 0 }); Vector2 screenPos = GetWorldToScreenScaled(headPos, cam, screenW, screenH);
    if (screenPos.x < 0 || screenPos.y < 0 || screenPos.x > screenW || screenPos.y > screenH) return;
    for (int i = 0; i < (int)logs.size(); i++) {
        float a = fminf(1.0f, logs[i].life * 2.0f); float moveUp = (4.0f - logs[i].life) * 15.0f; float yOffset = (i * 35.0f) + moveUp;
        Vector2 tSize = MeasureTextEx(font, logs[i].message.c_str(), 24, 1); Vector2 drawPos = { screenPos.x - tSize.x / 2, screenPos.y - 60 - yOffset };
        DrawTextEx(font, logs[i].message.c_str(), { drawPos.x + 1, drawPos.y + 1 }, 24, 1, Fade(BLACK, a)); DrawTextEx(font, logs[i].message.c_str(), drawPos, 24, 1, Fade(logs[i].color, a));
    }
}

// 頭上UIの描画関数
void UI::DrawOverheadUI(Player& p, std::vector<Enemy>& enemies, std::vector<DroppedItem>& di, Dungeon& d, Camera3D& cam, Font font, int screenW, int screenH) {
    // 1. 敵の頭上UI
    for (auto& e : enemies) {
        if (!d.IsDiscovered((float)e.position.x, (float)e.position.z)) continue;
        float dist = Vector3Distance(p.position, e.position);
        if (dist > 20.0f) continue;
        Vector3 headPos = Vector3Add(e.position, { 0.0f, 1.8f, 0.0f });
        Vector2 screenPos = GetWorldToScreenScaled(headPos, cam, screenW, screenH);
        if (screenPos.x < -100 || screenPos.x > screenW + 100 || screenPos.y < -100 || screenPos.y > screenH + 100) continue;
        float barWidth = 80.0f; float barHeight = 10.0f; float barX = screenPos.x - barWidth / 2; float barY = screenPos.y;
        DrawRectangle((int)barX - 2, (int)barY - 2, (int)barWidth + 4, (int)barHeight + 4, BLACK);
        DrawRectangle((int)barX, (int)barY, (int)barWidth, (int)barHeight, Fade(RED, 0.3f));
        float hpRatio = e.hp / e.maxHp;
        DrawRectangle((int)barX, (int)barY, (int)(barWidth * hpRatio), (int)barHeight, RED);
        std::string nameText = TextFormat("Lv.%d %s", e.level, e.data.name.c_str());
        Vector2 textSize = MeasureTextEx(font, nameText.c_str(), 20, 1);
        DrawTextEx(font, nameText.c_str(), { screenPos.x - textSize.x / 2, barY - 22 }, 20, 1, WHITE);
    }
    // 2. アイテムの頭上UI
    for (auto& item : di) {
        if (!d.IsDiscovered((float)item.pos.x, (float)item.pos.z)) continue;
        float dist = Vector3Distance(p.position, item.pos);
        if (dist > 15.0f) continue;
        Vector3 labelPos = Vector3Add(item.pos, { 0.0f, 0.8f, 0.0f });
        Vector2 screenPos = GetWorldToScreenScaled(labelPos, cam, screenW, screenH);
        if (screenPos.x < -50 || screenPos.x > screenW + 50 || screenPos.y < -50 || screenPos.y > screenH + 50) continue;
        std::string name = Player::GetFullItemName(item.data);
        Color nameCol = WHITE;
        if (item.data.type == "EQUIP" || item.data.type == "ARMOR") nameCol = GOLD;
        else if (item.data.type == "CONSUMABLE") nameCol = GREEN;
        else nameCol = LIGHTGRAY;
        int fontSize = (dist < 3.0f) ? 24 : 18;
        Vector2 textSize = MeasureTextEx(font, name.c_str(), (float)fontSize, 1);
        DrawRectangle((int)(screenPos.x - textSize.x / 2 - 4), (int)(screenPos.y - 4), (int)textSize.x + 8, (int)textSize.y + 8, Fade(BLACK, 0.4f));
        DrawTextEx(font, name.c_str(), { screenPos.x - textSize.x / 2, screenPos.y }, (float)fontSize, 1, nameCol);
    }
    // 3. プレイヤーの頭上UI (HPバー)
    {
        Vector3 headPos = Vector3Add(p.position, { 0.0f, 2.2f, 0.0f });
        Vector2 screenPos = GetWorldToScreenScaled(headPos, cam, screenW, screenH);
        if (screenPos.x >= 0 && screenPos.x <= screenW && screenPos.y >= 0 && screenPos.y <= screenH) {
            float barWidth = 100.0f; float barHeight = 12.0f;
            float barX = screenPos.x - barWidth / 2; float barY = screenPos.y;
            DrawRectangle((int)barX - 2, (int)barY - 2, (int)barWidth + 4, (int)barHeight + 4, BLACK);
            float hpRatio = p.hp / p.maxHp;
            DrawRectangle((int)barX, (int)barY, (int)(barWidth * hpRatio), (int)barHeight, GREEN);
        }
    }
}

int UI::DrawPrompt(const char* label, int sw, int sh, Font font) {
    std::string msg = DataManager::uiStrings[label]; if (msg.empty()) msg = label;
    int bw = 600, bh = 250, bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f)); DrawRectangleLines(bx, by, bw, bh, GOLD);
    Vector2 tS = MeasureTextEx(font, msg.c_str(), 32, 1); DrawTextEx(font, msg.c_str(), { (float)sw / 2 - tS.x / 2, (float)by + 60 }, 32, 1, WHITE);
    Rectangle bY = { (float)sw / 2 - 160, (float)by + 150, 140, 60 }, bN = { (float)sw / 2 + 20, (float)by + 150, 140, 60 };
    int res = 0; if (UI::DrawButton(bY, "YES", font, GREEN)) res = 1; if (UI::DrawButton(bN, "NO", font, RED)) res = 2; return res;
}