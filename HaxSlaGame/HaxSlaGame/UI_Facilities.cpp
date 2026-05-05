#include "UI.h"
#include "Player.h"
#include "Game.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "raymath.h"
#include <math.h>

static std::string T(const std::string& key, const std::string& def) {
    if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
    return def;
}

void UI::DrawCraftingMenu(Player& p, Font font, bool& isOpen) {
    static bool wasOpen = false; static float openTimer = 0.0f;
    if (isOpen && !wasOpen) openTimer = 0.0f; wasOpen = isOpen; if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, ORANGE);
    if (UI::DrawButton({ (float)sw - 160, 60, 100, 40 }, T("CLOSE", "Close").c_str(), font, locked ? Fade(RED, 0.5f) : RED) && !locked) isOpen = false;
    DrawTextEx(font, T("CRAFT", "Craft").c_str(), { 80, 70 }, 24, 1, ORANGE); DrawTextEx(font, TextFormat("Gold: %d", p.gold), { 250, 75 }, 20, 1, YELLOW);

    const int perPage = 6; int maxP = (int)ceil((float)DataManager::recipes.size() / perPage);
    for (int i = 0; i < perPage; i++) {
        int idx = craftingScroll * perPage + i; if (idx >= (int)DataManager::recipes.size()) break;
        auto& r = DataManager::recipes[idx]; ItemData res = DataManager::GetItemConfigCopy(r.resultItemId);

        float y = 120.0f + (float)i * 80.0f; Rectangle itemRect = { 80, (float)y, (float)sw - 200, 70 }; Rectangle createBtn = { (float)sw - 200, y + 15, 80, 40 };
        DrawRectangleRec(itemRect, Fade(DARKGRAY, showDetail ? 0.2f : 0.5f));
        if (!showDetail && !locked && CheckCollisionPointRec(GetMousePosition(), itemRect)) { if (!CheckCollisionPointRec(GetMousePosition(), createBtn)) { if (IsMouseButtonPressed(0)) OpenDetail(res); } }

        DrawTextEx(font, res.name.c_str(), { 90, y + 10 }, 20, 1, Player::GetItemRarityColor(res));

        std::string matStr = T("MATERIAL", "Material") + ": "; bool canCraft = true;
        for (auto& m : r.materials) {
            ItemData md = DataManager::GetItemConfigCopy(m.itemId); int playerHas = 0;
            for (auto& pi : p.inventoryItems) if (pi.id == m.itemId) playerHas = pi.count;
            if (playerHas < m.count) canCraft = false; matStr += TextFormat("%s %d/%d  ", md.name.c_str(), playerHas, m.count);
        }
        DrawTextEx(font, matStr.c_str(), { 90, y + 40 }, 14, 1, LIGHTGRAY); DrawTextEx(font, TextFormat(T("COST", "Cost: %d G").c_str(), r.cost), { (float)sw - 320, y + 25 }, 16, 1, (p.gold >= r.cost ? YELLOW : RED));

        if (canCraft && p.gold >= r.cost) {
            if (UI::DrawButton(createBtn, T("CREATE", "Create").c_str(), font, locked ? Fade(ORANGE, 0.5f) : ORANGE) && !locked) {
                p.gold -= r.cost;
                for (auto& m : r.materials) {
                    for (auto it = p.inventoryItems.begin(); it != p.inventoryItems.end(); ) {
                        if (it->id == m.itemId) { it->count -= m.count; if (it->count <= 0) it = p.inventoryItems.erase(it); else ++it; break; }
                        else ++it;
                    }
                }
                ItemData newItem = res; if (newItem.type == "EQUIP" || newItem.type == "ARMOR") newItem.modifierId = DataManager::GetRandomModifierId();
                p.AddToInventory(newItem); AudioManager::PlaySE(SE_REFORGE);
                UI::AddSystemLog(TextFormat("Crafted: %s", Player::GetFullItemName(newItem).c_str()), Player::GetItemRarityColor(newItem));
            }
        }
        else { DrawRectangle((int)sw - 200, (int)y + 15, 80, 40, showDetail ? ColorBrightness(GRAY, -0.4f) : GRAY); }
    }
    if (UI::DrawButton({ 80, 620, 100, 30 }, "<<", font, locked ? GRAY : GRAY) && !locked && craftingScroll > 0) craftingScroll--;
    if (UI::DrawButton({ 200, 620, 100, 30 }, ">>", font, locked ? GRAY : GRAY) && !locked && craftingScroll < maxP - 1) craftingScroll++;

    DrawDetailWindow(font);
}

void UI::DrawStorage(Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip) {
    static bool wasOpen = false; static float openTimer = 0.0f; static int transferIdx = -1; static bool isDeposit = true; static int transferAmount = 1; static int transferMax = 1;
    if (isOpen && !wasOpen) { openTimer = 0.0f; transferIdx = -1; } wasOpen = isOpen; if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f) || showDetail; bool modalLocked = locked || (transferIdx != -1);
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, GOLD);
    DrawTextEx(font, T("PLAYER_INV", "Player Inv").c_str(), { 80, 120 }, 20, 1, SKYBLUE);

    const int perP = 10; int mPInv = (int)ceil((float)p.inventoryItems.size() / perP); int mPBox = (int)ceil((float)sItems.size() / perP);
    if (mPInv < 1) mPInv = 1; if (mPBox < 1) mPBox = 1;

    for (int i = 0; i < perP; i++) {
        int idx = storageInvPage * perP + i; if (idx >= (int)p.inventoryItems.size()) break;
        Rectangle r = { 80, (float)170 + i * 42, 350, 38 }; DrawRectangleRec(r, modalLocked ? ColorBrightness(DARKGRAY, -0.4f) : DARKGRAY);
        DrawTextEx(font, TextFormat("%s x%d", p.inventoryItems[idx].name.c_str(), p.inventoryItems[idx].count), { r.x + 10, r.y + 10 }, 18, 1, Player::GetItemRarityColor(p.inventoryItems[idx]));
        if (!modalLocked && CheckCollisionPointRec(GetMousePosition(), r)) { if (IsMouseButtonPressed(0)) OpenDetail(p.inventoryItems[idx]); }
        if (UI::DrawButton({ 440, r.y, 110, 38 }, T("DEPOSIT", "Deposit >>").c_str(), font, modalLocked ? Fade(BLUE, 0.5f) : BLUE) && !modalLocked) { transferIdx = idx; isDeposit = true; transferAmount = 1; transferMax = p.inventoryItems[idx].count; AudioManager::PlaySE(SE_CLICK); }
    }
    if (UI::DrawButton({ 80, 600, 100, 30 }, "<<", font, modalLocked ? GRAY : GRAY) && !modalLocked && storageInvPage > 0) storageInvPage--;
    if (UI::DrawButton({ 190, 600, 100, 30 }, ">>", font, modalLocked ? GRAY : GRAY) && !modalLocked && storageInvPage < mPInv - 1) storageInvPage++;

    DrawTextEx(font, T("STORAGE_INV", "Storage Inv").c_str(), { 620, 120 }, 20, 1, GREEN);
    for (int i = 0; i < perP; i++) {
        int idx = storageBoxPage * perP + i; if (idx >= (int)sItems.size()) break;
        Rectangle r = { 770, (float)170 + i * 42, 350, 38 }; DrawRectangleRec(r, modalLocked ? ColorBrightness(DARKBLUE, -0.4f) : DARKBLUE);
        DrawTextEx(font, TextFormat("%s x%d", sItems[idx].name.c_str(), sItems[idx].count), { r.x + 10, r.y + 10 }, 18, 1, Player::GetItemRarityColor(sItems[idx]));
        if (!modalLocked && CheckCollisionPointRec(GetMousePosition(), r)) { if (IsMouseButtonPressed(0)) OpenDetail(sItems[idx]); }
        if (UI::DrawButton({ 620, r.y, 110, 38 }, T("WITHDRAW", "<< Withdraw").c_str(), font, modalLocked ? Fade(DARKGREEN, 0.5f) : DARKGREEN) && !modalLocked) { transferIdx = idx; isDeposit = false; transferAmount = 1; transferMax = sItems[idx].count; AudioManager::PlaySE(SE_CLICK); }
    }
    if (UI::DrawButton({ 620, 600, 100, 30 }, "<<", font, modalLocked ? GRAY : GRAY) && !modalLocked && storageBoxPage > 0) storageBoxPage--;
    if (UI::DrawButton({ 730, 600, 100, 30 }, ">>", font, modalLocked ? GRAY : GRAY) && !modalLocked && storageBoxPage < mPBox - 1) storageBoxPage++;

    if (UI::DrawButton({ (float)sw - 160, 70, 100, 45 }, T("CLOSE", "Close").c_str(), font, modalLocked ? Fade(RED, 0.5f) : RED) && !modalLocked) isOpen = false;

    if (transferIdx != -1 && !showDetail) {
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.7f));
        int mw = 420, mh = 260; int mx = sw / 2 - mw / 2, my = sh / 2 - mh / 2;
        DrawRectangle(mx, my, mw, mh, DARKGRAY); DrawRectangleLinesEx({ (float)mx, (float)my, (float)mw, (float)mh }, 3, ORANGE);

        ItemData& targetItem = isDeposit ? p.inventoryItems[transferIdx] : sItems[transferIdx];
        const char* title = isDeposit ? T("DEPOSIT_AMT", "Deposit Amount").c_str() : T("WITHDRAW_AMT", "Withdraw Amount").c_str();
        Vector2 tSize = MeasureTextEx(font, title, 20, 1); DrawTextEx(font, title, { (float)mx + mw / 2 - tSize.x / 2, (float)my + 20 }, 20, 1, WHITE);

        std::string nameStr = TextFormat("%s (Max: %d)", targetItem.name.c_str(), transferMax); Vector2 nSize = MeasureTextEx(font, nameStr.c_str(), 18, 1);
        DrawTextEx(font, nameStr.c_str(), { (float)mx + mw / 2 - nSize.x / 2, (float)my + 60 }, 18, 1, Player::GetItemRarityColor(targetItem));

        if (UI::DrawButton({ (float)mx + 80, (float)my + 110, 40, 40 }, "-", font, GRAY)) { if (transferAmount > 1) { transferAmount--; AudioManager::PlaySE(SE_CLICK); } }
        if (UI::DrawButton({ (float)mx + 300, (float)my + 110, 40, 40 }, "+", font, GRAY)) { if (transferAmount < transferMax) { transferAmount++; AudioManager::PlaySE(SE_CLICK); } }
        if (UI::DrawButton({ (float)mx + 30, (float)my + 110, 40, 40 }, "-10", font, DARKGRAY)) { transferAmount -= 10; if (transferAmount < 1) transferAmount = 1; AudioManager::PlaySE(SE_CLICK); }
        if (UI::DrawButton({ (float)mx + 350, (float)my + 110, 40, 40 }, "+10", font, DARKGRAY)) { transferAmount += 10; if (transferAmount > transferMax) transferAmount = transferMax; AudioManager::PlaySE(SE_CLICK); }

        std::string amtStr = std::to_string(transferAmount); Vector2 aSize = MeasureTextEx(font, amtStr.c_str(), 24, 1);
        DrawTextEx(font, amtStr.c_str(), { (float)mx + mw / 2 - aSize.x / 2, (float)my + 118 }, 24, 1, WHITE);

        if (UI::DrawButton({ (float)mx + 60, (float)my + 190, 120, 40 }, T("YES", "Confirm").c_str(), font, GREEN)) {
            if (isDeposit) {
                bool found = false; for (auto& si : sItems) { if (si.id == targetItem.id) { si.count += transferAmount; found = true; break; } }
                if (!found) { ItemData copy = targetItem; copy.count = transferAmount; sItems.push_back(copy); }
                p.inventoryItems[transferIdx].count -= transferAmount; if (p.inventoryItems[transferIdx].count <= 0) p.inventoryItems.erase(p.inventoryItems.begin() + transferIdx);
            }
            else {
                bool found = false; for (auto& pi : p.inventoryItems) { if (pi.id == targetItem.id) { pi.count += transferAmount; found = true; break; } }
                if (!found) { if (p.inventoryItems.size() < MAX_ITEM_TYPES) { ItemData copy = targetItem; copy.count = transferAmount; p.inventoryItems.push_back(copy); } else { found = true; } }
                if (found || p.inventoryItems.size() <= MAX_ITEM_TYPES) { sItems[transferIdx].count -= transferAmount; if (sItems[transferIdx].count <= 0) sItems.erase(sItems.begin() + transferIdx); }
            }
            transferIdx = -1; AudioManager::PlaySE(SE_CLICK);
        }
        if (UI::DrawButton({ (float)mx + 240, (float)my + 190, 120, 40 }, T("NO", "Cancel").c_str(), font, RED)) { transferIdx = -1; AudioManager::PlaySE(SE_CLICK); }
    }
    DrawDetailWindow(font);
}

void UI::DrawReforgeMenu(Player& p, Font font, bool& isOpen) {
    static bool wasOpen = false; static float openTimer = 0.0f;
    if (isOpen && !wasOpen) openTimer = 0.0f; wasOpen = isOpen; if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);

    int sw = GetScreenWidth(), sh = GetScreenHeight();
    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, GOLD);
    if (UI::DrawButton({ (float)sw - 160, 60, 100, 40 }, T("CLOSE", "Close").c_str(), font, locked ? Fade(RED, 0.5f) : RED) && !locked) { isOpen = false; reforgeItemIdx = -1; }
    DrawTextEx(font, T("REFORGE", "Reforge").c_str(), { 80, 70 }, 24, 1, GOLD); DrawTextEx(font, TextFormat("Gold: %d G", p.gold), { 80, 110 }, 20, 1, YELLOW);
    const int perPage = 10;

    for (int i = 0; i < (int)p.inventoryEquip.size(); i++) {
        if (i >= perPage) break;
        Rectangle r = { 80, 180.0f + i * 42, 350, 38 }; Color c = (reforgeItemIdx == i) ? DARKBLUE : DARKGRAY;
        if (DrawButton(r, "", font, locked ? Fade(c, 0.5f) : c) && !locked) { reforgeItemIdx = i; }
        Vector2 tSize = MeasureTextEx(font, Player::GetFullItemName(p.inventoryEquip[i]).c_str(), 18, 1);
        DrawTextEx(font, Player::GetFullItemName(p.inventoryEquip[i]).c_str(), { r.x + r.width / 2 - tSize.x / 2, r.y + r.height / 2 - tSize.y / 2 }, 18, 1, Player::GetItemRarityColor(p.inventoryEquip[i]));
    }
    if (reforgeItemIdx != -1 && reforgeItemIdx < (int)p.inventoryEquip.size()) {
        ItemData& item = p.inventoryEquip[reforgeItemIdx];
        DrawRectangle(500, 180, 400, 300, Fade(DARKGRAY, 0.5f));
        DrawTextEx(font, Player::GetFullItemName(item).c_str(), { 520, 200 }, 22, 1, Player::GetItemRarityColor(item));
        float baseAtk = item.atkBonus; Modifier mod = DataManager::GetModifier(item.modifierId); float modAtk = mod.atk; float totalAtk = baseAtk + modAtk;
        DrawTextEx(font, TextFormat(T("BASE_ATK", "Base ATK: %.1f").c_str(), baseAtk), { 520, 250 }, 18, 1, SKYBLUE);
        DrawTextEx(font, TextFormat(T("MOD_ATK", "Mod ATK: %.1f (%s)").c_str(), modAtk, mod.name.c_str()), { 520, 280 }, 18, 1, (modAtk >= 0 ? GREEN : RED));
        DrawTextEx(font, TextFormat(T("TOTAL_ATK", "Total ATK: %.1f").c_str(), totalAtk), { 520, 310 }, 20, 1, YELLOW);
        int cost = 50 + p.level * 10 + (int)item.atkBonus * 2; DrawTextEx(font, TextFormat(T("COST", "Cost: %d G").c_str(), cost), { 520, 360 }, 18, 1, GOLD);
        if (p.gold >= cost) {
            if (DrawButton({ 520, 400, 150, 50 }, T("REFORGE", "Reforge").c_str(), font, locked ? Fade(GREEN, 0.5f) : GREEN) && !locked) {
                p.gold -= cost; item.modifierId = DataManager::GetRandomModifierId(); AudioManager::PlaySE(SE_REFORGE);
            }
        }
        else { DrawRectangle(520, 400, 150, 50, showDetail ? ColorBrightness(GRAY, -0.4f) : GRAY); DrawTextEx(font, T("NOT_ENOUGH_GOLD", "Not Enough Gold").c_str(), { 530, 415 }, 18, 1, BLACK); }
    }
    DrawDetailWindow(font);
}

void UI::DrawWarpMenu(void* gamePtr, int unlockedDungeon, const std::vector<int>& maxFloors, Font font, bool& isOpen) {
    Game* game = (Game*)gamePtr;
    static bool wasOpen = false; static float openTimer = 0.0f;
    if (isOpen && !wasOpen) openTimer = 0.0f; wasOpen = isOpen; if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    DrawRectangle(250, 50, sw - 500, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLines(250, 50, sw - 500, sh - 100, SKYBLUE);
    DrawTextEx(font, T("WARP_PORTAL", "Warp Portal").c_str(), { 280, 70 }, 24, 1, WHITE);
    if (UI::DrawButton({ (float)sw - 370, 65, 100, 40 }, T("CLOSE", "Close").c_str(), font, locked ? Fade(RED, 0.5f) : RED) && !locked) { isOpen = false; return; }

    std::string dn1 = T("DUNGEON_1", "Dungeon 1");
    std::string dn2 = T("DUNGEON_2", "Dungeon 2");
    std::string dn3 = T("DUNGEON_3", "Abyss");
    const char* dNames[] = { dn1.c_str(), dn2.c_str(), dn3.c_str() };
    for (int i = 0; i < 3; i++) {
        Rectangle tabR = { 280.0f + i * 210, 130, 200, 40 }; Color c = (selectedDungeonTab == i) ? DARKBLUE : DARKGRAY;
        if (i > unlockedDungeon) c = Fade(BLACK, 0.5f);
        if (UI::DrawButton(tabR, dNames[i], font, c) && !locked && i <= unlockedDungeon) { selectedDungeonTab = i; warpScroll = 0; }
        if (i > unlockedDungeon) { DrawTextEx(font, T("LOCKED", "LOCKED").c_str(), { tabR.x + 60, tabR.y + 10 }, 18, 1, GRAY); }
    }

    int maxF = maxFloors[selectedDungeonTab]; std::vector<int> warpFloors; for (int i = 5; i <= maxF; i += 5) warpFloors.push_back(i);
    if (warpFloors.empty()) { DrawTextEx(font, T("NO_WARP", "No Warp Available").c_str(), { 300, 250 }, 20, 1, GRAY); }
    else {
        const int perPage = 7; int maxP = (int)ceil((float)warpFloors.size() / perPage);
        for (int i = 0; i < perPage; i++) {
            int idx = warpScroll * perPage + i; if (idx >= (int)warpFloors.size()) break;
            int f = warpFloors[idx]; std::string label = TextFormat(T("FLOOR_INFO", "Floor %d").c_str(), f); if (f % 10 == 0) label += T("BOSS", " (Boss)"); else if (f % 10 == 5) label += T("SAFE", " (Safe)");
            Rectangle r = { 300.0f, 200.0f + i * 55, (float)sw - 600.0f, 45 };
            if (UI::DrawButton(r, label.c_str(), font, locked ? Fade(DARKGREEN, 0.5f) : DARKGREEN) && !locked) { game->WarpToFloor(selectedDungeonTab, f); }
        }
        float bottomY = sh - 120.0f;
        if (UI::DrawButton({ 300, bottomY, 100, 30 }, "<<", font, locked ? GRAY : GRAY) && !locked && warpScroll > 0) warpScroll--;
        if (UI::DrawButton({ (float)sw - 400, bottomY, 100, 30 }, ">>", font, locked ? GRAY : GRAY) && !locked && warpScroll < maxP - 1) warpScroll++;
    }
}

void UI::DrawQuestMenu(Player& p, Font font, bool& isOpen) {
    static bool wasOpen = false; static float openTimer = 0.0f;
    if (isOpen && !wasOpen) { openTimer = 0.0f; questScroll = 0; } wasOpen = isOpen; if (isOpen) openTimer += GetFrameTime();
    bool locked = (openTimer < 0.3f);
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    DrawRectangle(50, 50, sw - 100, sh - 100, Fade(BLACK, 0.95f)); DrawRectangleLinesEx({ 50, 50, (float)sw - 100, (float)sh - 100 }, 3, GREEN);
    if (UI::DrawButton({ (float)sw - 160, 60, 100, 40 }, T("CLOSE", "Close").c_str(), font, locked ? Fade(RED, 0.5f) : RED) && !locked) isOpen = false;
    DrawTextEx(font, T("QUESTS", "Quests").c_str(), { 80, 70 }, 24, 1, GREEN);

    const int perPage = 5; int totalQuests = (int)DataManager::quests.size();
    if (totalQuests == 0) { DrawTextEx(font, T("NO_QUESTS", "No Quests Available.").c_str(), { 80, 150 }, 20, 1, GRAY); return; }
    int maxP = (int)ceil((float)totalQuests / perPage);

    for (int i = 0; i < perPage; i++) {
        int idx = questScroll * perPage + i; if (idx >= totalQuests) break;
        auto& q = DataManager::quests[idx]; float y = 130.0f + (float)i * 95.0f;
        Rectangle qRect = { 80, y, (float)sw - 200, 85 }; DrawRectangleRec(qRect, Fade(DARKGRAY, 0.5f));
        DrawTextEx(font, q.title.c_str(), { 90, y + 10 }, 22, 1, GOLD); DrawTextEx(font, q.description.c_str(), { 90, y + 35 }, 16, 1, WHITE);

        bool isCleared = false; for (int cid : p.clearedQuests) { if (cid == q.id) { isCleared = true; break; } }
        int activeIdx = -1; for (int j = 0; j < (int)p.activeQuests.size(); j++) { if (p.activeQuests[j].questId == q.id) { activeIdx = j; break; } }
        Rectangle btnRect = { (float)sw - 220, y + 20, 100, 45 };

        if (isCleared) { DrawTextEx(font, T("CLEARED", "Cleared").c_str(), { (float)sw - 200, y + 30 }, 20, 1, GRAY); }
        else if (activeIdx != -1) {
            auto& pq = p.activeQuests[activeIdx];
            if (q.type == QUEST_HUNT) {
                DrawTextEx(font, TextFormat(T("HUNT_PROG", "Hunt: %d / %d").c_str(), pq.currentCount, q.targetCount), { 90, y + 60 }, 16, 1, SKYBLUE);
                if (pq.isCompleted) { if (UI::DrawButton(btnRect, T("REPORT", "Report").c_str(), font, locked ? Fade(ORANGE, 0.5f) : ORANGE) && !locked) { p.CompleteQuest(q.id); AudioManager::PlaySE(SE_CLICK); } }
                else { DrawTextEx(font, T("IN_PROGRESS", "In Progress").c_str(), { (float)sw - 200, y + 30 }, 20, 1, YELLOW); }
            }
            else if (q.type == QUEST_GATHER) {
                int currentInvCount = 0; for (const auto& it : p.inventoryItems) { if (it.id == q.targetId) currentInvCount += it.count; }
                DrawTextEx(font, TextFormat(T("GATHER_PROG", "Gather: %d / %d").c_str(), currentInvCount, q.targetCount), { 90, y + 60 }, 16, 1, GREEN);
                if (currentInvCount >= q.targetCount) { if (UI::DrawButton(btnRect, T("REPORT", "Report").c_str(), font, locked ? Fade(ORANGE, 0.5f) : ORANGE) && !locked) { p.CompleteQuest(q.id); AudioManager::PlaySE(SE_CLICK); } }
                else { DrawTextEx(font, T("IN_PROGRESS", "In Progress").c_str(), { (float)sw - 200, y + 30 }, 20, 1, YELLOW); }
            }
        }
        else {
            std::string reqStr = (q.type == QUEST_HUNT) ? T("HUNT", "Hunt") : T("GATHER", "Gather");
            DrawTextEx(font, TextFormat(T("TARGET", "Target: %s %d").c_str(), reqStr.c_str(), q.targetCount), { 90, y + 60 }, 16, 1, LIGHTGRAY);
            if (UI::DrawButton(btnRect, T("ACCEPT", "Accept").c_str(), font, locked ? Fade(BLUE, 0.5f) : BLUE) && !locked) {
                PlayerQuest newQ; newQ.questId = q.id; newQ.currentCount = 0; newQ.isCompleted = false; p.activeQuests.push_back(newQ); AudioManager::PlaySE(SE_CLICK);
                UI::AddSystemLog("Accepted Quest: " + q.title, YELLOW);
            }
        }
    }
    if (UI::DrawButton({ 80, 620, 100, 30 }, "<<", font, locked ? GRAY : GRAY) && !locked && questScroll > 0) questScroll--;
    if (UI::DrawButton({ 200, 620, 100, 30 }, ">>", font, locked ? GRAY : GRAY) && !locked && questScroll < maxP - 1) questScroll++;
}