#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "Game.h"
#include "raymath.h"
#include <math.h>

static std::string T(const std::string& key, const std::string& def) {
    if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
    return def;
}

int UI::itemPage = 0; int UI::equipPage = 0; int UI::debugPage = 0;
int UI::storageInvPage = 0; int UI::storageBoxPage = 0; int UI::itemSubTab = 0;
int UI::reforgeItemIdx = -1;
int UI::warpScroll = 0;
int UI::craftingScroll = 0;
int UI::selectedDungeonTab = 0;
int UI::questScroll = 0;
Vector2 UI::skillOffset = { 0.0f, 0.0f };
Vector2 UI::mapOffset = { 0.0f, 0.0f };

bool UI::showDetail = false;
ItemData UI::focusingItem;
float UI::detailOpenTimer = 0.0f;
int UI::deleteConfirmSlot = 0;

std::vector<SystemLogMessage> UI::systemLogs;

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

    int sw = GetScreenWidth(); int sh = GetScreenHeight();
    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.7f));

    int w = 450; int h = 550;
    int x = (sw - w) / 2; int y = (sh - h) / 2;

    DrawRectangle(x, y, w, h, Fade(DARKBLUE, 0.95f));
    DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 3, GOLD);

    Color rarityColor = Player::GetItemRarityColor(focusingItem);
    DrawTextEx(font, Player::GetFullItemName(focusingItem).c_str(), { (float)x + 25, (float)y + 25 }, 28, 1, rarityColor);

    std::string typeStr = focusingItem.type;
    if (typeStr == "EQUIP") typeStr = T("WEAPON", "Weapon");
    else if (typeStr == "ARMOR") typeStr = T("ARMOR_TYPE", "Armor");
    else if (typeStr == "CONSUMABLE") typeStr = T("CONSUMABLE", "Consumable");
    else if (typeStr == "MATERIAL") typeStr = T("MATERIAL", "Material");

    if (focusingItem.type == "EQUIP") {
        std::string wTypes[] = { T("SWORD", "Sword"), T("SPEAR", "Spear"), T("AXE", "Axe"), T("WAND", "Wand"), T("NONE", "None") };
        if (focusingItem.weaponSubtype >= 0 && focusingItem.weaponSubtype <= 3) typeStr += std::string(" (") + wTypes[focusingItem.weaponSubtype] + ")";
    }
    else if (focusingItem.type == "ARMOR") {
        std::string aTypes[] = { T("HEAD", "Head"), T("CHEST", "Chest"), T("HANDS", "Hands"), T("LEGS", "Legs"), T("FEET", "Feet") };
        if (focusingItem.weaponSubtype >= 0 && focusingItem.weaponSubtype <= 4) typeStr += std::string(" (") + aTypes[focusingItem.weaponSubtype] + ")";
    }
    DrawTextEx(font, typeStr.c_str(), { (float)x + 25, (float)y + 60 }, 20, 1, LIGHTGRAY);

    Modifier mod = DataManager::GetModifier(focusingItem.modifierId);
    float totalAtk = focusingItem.atkBonus + mod.atk; float totalDef = focusingItem.defBonus + mod.def;
    float totalHp = focusingItem.hpBonus + mod.hp; float totalSpd = focusingItem.speedBonus + mod.spd;

    int statsY = y + 110; int lineH = 35;
    if (totalAtk != 0) { DrawTextEx(font, TextFormat(T("STAT_ATK", "ATK : %+.1f").c_str(), totalAtk), { (float)x + 40, (float)statsY }, 22, 1, RED); if (mod.atk != 0) DrawTextEx(font, TextFormat(T("STAT_MOD", "(Mod %+.1f)").c_str(), mod.atk), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (totalDef != 0) { DrawTextEx(font, TextFormat(T("STAT_DEF", "DEF : %+.1f").c_str(), totalDef), { (float)x + 40, (float)statsY }, 22, 1, BLUE); if (mod.def != 0) DrawTextEx(font, TextFormat(T("STAT_MOD", "(Mod %+.1f)").c_str(), mod.def), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (totalHp != 0) { DrawTextEx(font, TextFormat(T("STAT_HP", "HP : %+.0f").c_str(), totalHp), { (float)x + 40, (float)statsY }, 22, 1, GREEN); if (mod.hp != 0) DrawTextEx(font, TextFormat(T("STAT_MOD_HP", "(Mod %+.0f)").c_str(), mod.hp), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (totalSpd != 0) { DrawTextEx(font, TextFormat(T("STAT_SPD", "SPD : %+.2f").c_str(), totalSpd), { (float)x + 40, (float)statsY }, 22, 1, SKYBLUE); if (mod.spd != 0) DrawTextEx(font, TextFormat(T("STAT_MOD_SPD", "(Mod %+.2f)").c_str(), mod.spd), { (float)x + 250, (float)statsY }, 18, 1, ORANGE); statsY += lineH; }
    if (focusingItem.heal > 0) { DrawTextEx(font, TextFormat(T("STAT_HEAL", "Heal : %.0f").c_str(), focusingItem.heal), { (float)x + 40, (float)statsY }, 22, 1, PINK); statsY += lineH; }

    if (mod.id != 0) {
        statsY += 20; DrawRectangleLines(x + 20, statsY - 5, w - 40, 70, ORANGE);
        DrawTextEx(font, T("ENCHANTMENT", "Enchantment:").c_str(), { (float)x + 30, (float)statsY }, 18, 1, ORANGE);
        DrawTextEx(font, mod.name.c_str(), { (float)x + 50, (float)statsY + 30 }, 22, 1, YELLOW);
    }

    Rectangle closeBtn = { (float)x + w / 2 - 80, (float)y + h - 70, 160, 50 };
    bool inputEnabled = (detailOpenTimer >= 0.3f); bool hover = inputEnabled && CheckCollisionPointRec(GetMousePosition(), closeBtn);
    Color btnCol = hover ? RED : MAROON; if (!inputEnabled) btnCol = Fade(MAROON, 0.5f);
    DrawRectangleRec(closeBtn, btnCol); DrawRectangleLinesEx(closeBtn, 2, WHITE);
    Vector2 txtSz = MeasureTextEx(font, T("CLOSE", "Close").c_str(), 24, 1);
    DrawTextEx(font, T("CLOSE", "Close").c_str(), { closeBtn.x + closeBtn.width / 2 - txtSz.x / 2, closeBtn.y + closeBtn.height / 2 - txtSz.y / 2 }, 24, 1, inputEnabled ? WHITE : GRAY);
    if (hover && IsMouseButtonPressed(0)) { showDetail = false; AudioManager::PlaySE(SE_CLICK); }
}

int UI::DrawPrompt(const char* label, int sw, int sh, Font font) {
    std::string msg = T(label, label);
    int bw = 450, bh = 180, bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;
    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f)); DrawRectangleLines(bx, by, bw, bh, GOLD);
    Vector2 tS = MeasureTextEx(font, msg.c_str(), 24, 1); DrawTextEx(font, msg.c_str(), { (float)sw / 2 - tS.x / 2, (float)by + 40 }, 24, 1, WHITE);
    Rectangle bY = { (float)sw / 2 - 140, (float)by + 100, 120, 50 }, bN = { (float)sw / 2 + 20, (float)by + 100, 120, 50 };
    int res = 0; if (UI::DrawButton(bY, T("YES", "YES").c_str(), font, GREEN)) res = 1; if (UI::DrawButton(bN, T("NO", "NO").c_str(), font, RED)) res = 2; return res;
}

int UI::DrawTitleScreen(Font font) {
    int sw = GetScreenWidth(); int sh = GetScreenHeight();

    if (DataManager::titleBg.id != 0) {
        Rectangle source = { 0.0f, 0.0f, (float)DataManager::titleBg.width, (float)DataManager::titleBg.height };
        Rectangle dest = { 0.0f, 0.0f, (float)sw, (float)sh };
        Vector2 origin = { 0.0f, 0.0f };
        DrawTexturePro(DataManager::titleBg, source, dest, origin, 0.0f, WHITE);
    }
    else {
        DrawRectangleGradientV(0, 0, sw, sh, DARKBLUE, BLACK);
        const char* title = "3D Hack & Slash";
        Vector2 tSize = MeasureTextEx(font, title, 60, 2);
        DrawTextEx(font, title, { (float)(sw - tSize.x) / 2, 100.0f }, 60, 2, GOLD);
    }

    if (deleteConfirmSlot > 0) {
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));
        int w = 500, h = 250; int x = (sw - w) / 2, y = (sh - h) / 2;
        DrawRectangle(x, y, w, h, DARKGRAY); DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 3, RED);
        DrawTextEx(font, TextFormat(T("DELETE_CONFIRM", "Delete Slot %d ?").c_str(), deleteConfirmSlot), { (float)x + 40, (float)y + 60 }, 24, 1, WHITE);
        DrawTextEx(font, T("CANNOT_UNDONE", "Cannot be undone.").c_str(), { (float)x + 80, (float)y + 100 }, 20, 1, RED);
        if (DrawButton({ (float)x + 50, (float)y + 160, 150, 50 }, T("DELETE", "Delete").c_str(), font, RED)) { DataManager::DeleteSave(deleteConfirmSlot); deleteConfirmSlot = 0; }
        if (DrawButton({ (float)x + 300, (float)y + 160, 150, 50 }, T("CANCEL", "Cancel").c_str(), font, GRAY)) { deleteConfirmSlot = 0; }
        return 0;
    }

    int selectedSlot = 0;
    for (int i = 1; i <= 3; i++) {
        SaveHeader h = DataManager::GetSaveHeader(i);
        float y = 350.0f + (float)(i - 1) * 100.0f;
        Rectangle r = { (float)sw / 2 - 200, y, 400.0f, 80.0f };
        std::string label; Color c;

        if (h.exists) {
            if (h.isPortfolioMode) { label = TextFormat(T("SLOT_RUSH", "Slot %d:[RUSH] Lv.%d").c_str(), i, h.playerLevel); c = Fade(GOLD, 0.8f); }
            else { label = TextFormat(T("SLOT_DATA", "Slot %d: Lv.%d  Floor %d").c_str(), i, h.playerLevel, h.floor); c = Fade(DARKGREEN, 0.8f); }
        }
        else { label = TextFormat(T("SLOT_EMPTY", "Slot %d: (NO DATA)").c_str(), i); c = Fade(DARKGRAY, 0.8f); }

        if (DrawButton(r, label.c_str(), font, c)) { selectedSlot = i; }
        if (h.exists) { Rectangle delBtn = { r.x + r.width + 20, r.y + 20, 60, 40 }; if (DrawButton(delBtn, "X", font, Fade(MAROON, 0.8f))) { deleteConfirmSlot = i; } }
    }
    if (DrawButton({ 20, (float)sh - 110, 340, 40 }, T("PORTFOLIO_MODE", "Portfolio: 3-Floor Rush").c_str(), font, Fade(ORANGE, 0.8f))) return 888;
    if (DrawButton({ 20, (float)sh - 60, 200, 40 }, T("DEBUG_ROOM", "Debug Room").c_str(), font, Fade(PURPLE, 0.8f))) return 999;
    return selectedSlot;
}

void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, int dungeonId, bool debug, Font font) {
    int sw = GetScreenWidth(), sh = GetScreenHeight();

    std::string floorText;
    if (floor > 1000) { floorText = TextFormat(T("STAGE", "STAGE %d").c_str(), floor - 1000); }
    else if (floor == 0) { floorText = T("HOME", "HOME"); }
    else {
        std::string label = T("FLOOR", "Floor");
        std::string dName = T("DUNGEON_1", "Dungeon 1"); if (dungeonId == 1) dName = T("DUNGEON_2", "Dungeon 2"); else if (dungeonId == 2) dName = T("DUNGEON_3", "Abyss");
        floorText = TextFormat("%s: %s %d", dName.c_str(), label.c_str(), floor);
    }
    Vector2 fSize = MeasureTextEx(font, floorText.c_str(), 24, 1);
    DrawRectangle(20, 20, (int)fSize.x + 30, 40, Fade(BLACK, 0.6f)); DrawTextEx(font, floorText.c_str(), { 35, 28 }, 24, 1, WHITE);

    if (!p.activeQuests.empty()) {
        int questY = 75; int questX = 20;
        DrawTextEx(font, T("QUESTS", "[Active Quests]").c_str(), { (float)questX, (float)questY }, 18, 1, GOLD);
        questY += 22;
        for (const auto& q : p.activeQuests) {
            QuestData qData = DataManager::GetQuestData(q.questId);
            if (qData.id != -1) {
                std::string progressStr; Color textColor = WHITE;
                if (q.isCompleted) { progressStr = T("DONE", "[Done]"); textColor = GREEN; }
                else {
                    if (qData.type == QUEST_HUNT) { progressStr = TextFormat(" [%d/%d]", q.currentCount, qData.targetCount); }
                    else if (qData.type == QUEST_GATHER) {
                        int currentInvCount = 0; for (const auto& it : p.inventoryItems) { if (it.id == qData.targetId) currentInvCount += it.count; }
                        progressStr = TextFormat(" [%d/%d]", currentInvCount, qData.targetCount);
                        if (currentInvCount >= qData.targetCount) { progressStr += T("DONE", " [Done]"); textColor = GREEN; }
                    }
                }
                std::string displayText = qData.title + progressStr;
                Vector2 tSize = MeasureTextEx(font, displayText.c_str(), 16, 1);
                DrawRectangle(questX - 5, questY - 2, (int)tSize.x + 10, 20, Fade(BLACK, 0.5f));
                DrawTextEx(font, displayText.c_str(), { (float)questX, (float)questY }, 16, 1, textColor);
                questY += 22;
            }
        }
    }

    int mapSize = 200; int mapX = sw - mapSize - 20; int mapY = 20;
    DrawRectangle(mapX, mapY, mapSize, mapSize, Fade(BLACK, 0.6f)); DrawRectangleLines(mapX, mapY, mapSize, mapSize, GRAY);
    BeginScissorMode(mapX, mapY, mapSize, mapSize);
    float sc = 8.0f; float offX = mapX + mapSize / 2.0f - (p.position.x / TILE_SIZE) * sc; float offY = mapY + mapSize / 2.0f - (p.position.z / TILE_SIZE) * sc;
    for (int y = 0; y < d.currentHeight; y++) {
        for (int x = 0; x < d.currentWidth; x++) {
            if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) {
                Color tileCol = d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN;
                DrawRectangle(offX + x * sc, offY + y * sc, sc - 1, sc - 1, tileCol);
            }
        }
    }
    auto drawMapIcon = [&](Vector3 pos, Color col) { if (pos.x != -999 && d.IsDiscovered(pos.x, pos.z)) { DrawRectangle(offX + (pos.x / TILE_SIZE) * sc, offY + (pos.z / TILE_SIZE) * sc, sc, sc, col); } };
    drawMapIcon(d.stairsDownPos, GOLD); drawMapIcon(d.stairsUpPos, SKYBLUE); drawMapIcon(d.portalPos, PURPLE);
    if (d.isHome) {
        for (int i = 0; i < 3; i++) drawMapIcon(d.dungeonEntrances[i], GOLD);
        drawMapIcon(d.storageBoxPos, BROWN); drawMapIcon(d.reforgeStationPos, PURPLE); drawMapIcon(d.craftStationPos, ORANGE); drawMapIcon(d.questBoardPos, BEIGE);
    }
    else { drawMapIcon(d.healStationPos, PINK); drawMapIcon(d.bossSpawnPos, MAGENTA); }
    DrawCircle(mapX + mapSize / 2, mapY + mapSize / 2, 4, RED); EndScissorMode();

    int listCount = 0;
    for (auto& e : enemies) {
        if (e.hudTimer > 0) {
            int yPos = mapY + mapSize + 20 + listCount * 50;
            DrawRectangle(sw - 220, yPos, 200, 45, Fade(BLACK, 0.7f)); DrawTextEx(font, TextFormat("Lv.%d %s", e.level, e.data.name.c_str()), { (float)sw - 210, (float)yPos + 5 }, 16, 1, WHITE);
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
    DrawTextEx(font, TextFormat(T("HUD_LV_EXP", "Lv: %d   EXP: %d/%d").c_str(), p.level, p.exp, p.expToNext), { 20, (float)sh - 110 }, 18, 1, SKYBLUE);
    DrawRectangle(20, sh - 85, 280, 18, DARKGRAY); DrawRectangle(20, sh - 85, (int)(280 * (fmaxf(0.0f, p.hp) / p.maxHp)), 18, GREEN);
    DrawTextEx(font, TextFormat(T("HUD_HP", "HP: %.0f/%.0f").c_str(), p.hp, p.maxHp), { 30, (float)sh - 84 }, 14, 1, WHITE);
    DrawTextEx(font, TextFormat(T("HUD_STATS", "ATK: %.1f  DEF: %.1f").c_str(), p.attackPower, p.defense), { 20, (float)sh - 60 }, 18, 1, WHITE);
    DrawTextEx(font, TextFormat(T("HUD_GOLD_SP", "Gold: %d  SP: %d").c_str(), p.gold, p.skillPoints), { 20, (float)sh - 35 }, 18, 1, WHITE);

    int iconSize = 40; int startX = sw - 320; int startY = sh - 60;
    struct SkillIcon { SkillType type; const char* labelKey; const char* key; };
    SkillIcon icons[] = {
        { SKILL_ACTIVE_SMASH, "SMASH", "1" },
        { SKILL_ACTIVE_KONGO, "KONGO", "2" },
        { SKILL_ACTIVE_ZOUKYOU, "ATKUP", "3" },
        { SKILL_ACTIVE_STEALTH, "HIDE", "4" },
        { SKILL_ACTIVE_HEAL, "HEAL", "5" },
        { SKILL_ACTIVE_DASH, "DASH", "Shift" }
    };

    for (int i = 0; i < 6; i++) {
        int x = startX + i * (iconSize + 10); bool unlocked = p.IsSkillUnlocked(icons[i].type); Color baseCol = unlocked ? DARKBLUE : DARKGRAY;
        DrawRectangle(x, startY, iconSize, iconSize, baseCol); DrawRectangleLines(x, startY, iconSize, iconSize, RAYWHITE); DrawTextEx(font, icons[i].key, { (float)x + 2, (float)startY + 2 }, 10, 1, WHITE);
        if (unlocked) {
            float cd = p.GetSkillCooldown(icons[i].type); float maxCd = p.GetSkillMaxCooldown(icons[i].type);
            if (cd > 0) { float ratio = cd / maxCd; DrawRectangle(x, startY + (int)((float)iconSize * (1.0f - ratio)), iconSize, (int)((float)iconSize * ratio), Fade(RED, 0.7f)); DrawTextEx(font, TextFormat("%.1f", cd), { (float)x + 5, (float)startY + 15 }, 14, 1, YELLOW); }
            else { std::string label = T(icons[i].labelKey, icons[i].labelKey); DrawTextEx(font, label.c_str(), { (float)x + 2, (float)startY + 25 }, 10, 1, GREEN); }
        }
        else { DrawTextEx(font, T("SKILL_LOCKED", "LOCK").c_str(), { (float)x + 5, (float)startY + 15 }, 10, 1, GRAY); }
    }
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
        if (!d.IsDiscovered(item.pos.x, item.pos.z)) continue; float dist = Vector3Distance(p.position, item.pos);
        if (dist < 5.0f) {
            Vector2 s = GetWorldToScreen(item.pos, cam);
            if (s.x > 0 && s.y > 0) { DrawTextEx(font, Player::GetFullItemName(item.data).c_str(), { s.x - 20, s.y - 20 }, 16, 1, Player::GetItemRarityColor(item.data)); }
        }
    }
}

void UI::AddSystemLog(const std::string& text, Color color) {
    SystemLogMessage msg; msg.text = text; msg.color = color; msg.lifeTime = 5.0f; msg.maxLifeTime = 5.0f;
    systemLogs.push_back(msg); if (systemLogs.size() > 10) { systemLogs.erase(systemLogs.begin()); }
}
void UI::UpdateSystemLogs(float deltaTime) {
    for (auto it = systemLogs.begin(); it != systemLogs.end(); ) { it->lifeTime -= deltaTime; if (it->lifeTime <= 0.0f) { it = systemLogs.erase(it); } else { ++it; } }
}
void UI::DrawSystemLogs(Font font) {
    int startY = GetScreenHeight() - 150; int startX = 20; int fontSize = 20; int lineSpacing = 25;
    for (size_t i = 0; i < systemLogs.size(); ++i) {
        float alpha = 1.0f; if (systemLogs[i].lifeTime < 1.0f) { alpha = systemLogs[i].lifeTime; }
        Color textColor = systemLogs[i].color; textColor.a = static_cast<unsigned char>(255 * alpha);
        Vector2 tSize = MeasureTextEx(font, systemLogs[i].text.c_str(), fontSize, 1);
        int drawY = startY - static_cast<int>((systemLogs.size() - 1 - i) * lineSpacing);
        Color bgColor = BLACK; bgColor.a = static_cast<unsigned char>(150 * alpha);
        DrawRectangle(startX - 5, drawY - 2, static_cast<int>(tSize.x) + 10, fontSize + 4, bgColor);
        DrawTextEx(font, systemLogs[i].text.c_str(), { (float)startX, (float)drawY }, fontSize, 1, textColor);
    }
}