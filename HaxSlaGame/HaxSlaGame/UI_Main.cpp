#include "UI.h"
#include "Player.h"
#include "Enemy.h"
#include "Dungeon.h"
#include "DataManager.h"
#include "AudioManager.h"
#include "Game.h"
#include "raymath.h"
#include <math.h>

// ==========================================
// ユーティリティ関数
// ==========================================

static std::string T(const std::string& key, const std::string& def) {
    if (DataManager::uiStrings.count(key)) return DataManager::uiStrings[key];
    return def;
}

static std::string getKeyStr(int key) {
    if (key == KEY_LEFT_SHIFT || key == KEY_RIGHT_SHIFT) return "Shift";
    if (key == KEY_SPACE) return "Space";
    if (key >= 32 && key <= 126) return std::string(1, (char)key);
    return "Key";
}

std::string UI::GetPadBtnStr(int btn) {
    switch (btn) {
    case GAMEPAD_BUTTON_LEFT_FACE_UP: return "D-Up";
    case GAMEPAD_BUTTON_LEFT_FACE_RIGHT: return "D-Right";
    case GAMEPAD_BUTTON_LEFT_FACE_DOWN: return "D-Down";
    case GAMEPAD_BUTTON_LEFT_FACE_LEFT: return "D-Left";
    case GAMEPAD_BUTTON_RIGHT_FACE_UP: return "Y";
    case GAMEPAD_BUTTON_RIGHT_FACE_RIGHT: return "B";
    case GAMEPAD_BUTTON_RIGHT_FACE_DOWN: return "A";
    case GAMEPAD_BUTTON_RIGHT_FACE_LEFT: return "X";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_1: return "LB";
    case GAMEPAD_BUTTON_LEFT_TRIGGER_2: return "LT";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_1: return "RB";
    case GAMEPAD_BUTTON_RIGHT_TRIGGER_2: return "RT";
    case GAMEPAD_BUTTON_MIDDLE_RIGHT: return "Start";
    case GAMEPAD_BUTTON_MIDDLE_LEFT: return "Select";
    case GAMEPAD_BUTTON_LEFT_THUMB: return "LS";
    case GAMEPAD_BUTTON_RIGHT_THUMB: return "RS";
    }
    return "Btn_" + std::to_string(btn);
}

// ==========================================
// UI関連の静的変数（ステート管理用）の初期化
// ==========================================
int UI::itemPage = 0;
int UI::equipPage = 0;
int UI::storageInvPage = 0;
int UI::storageBoxPage = 0;
int UI::itemSubTab = 0;
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
std::vector<Rectangle> UI::interactables;

// ==========================================
// ゲームパッドによるUIナビゲーション (スナップ移動)
// ==========================================

void UI::ClearInteractables() {
    interactables.clear();
}

void UI::RegisterInteractable(Rectangle r) {
    interactables.push_back(r);
}

void UI::UpdatePadNavigation() {
    if (!IsGamepadAvailable(0)) return;

    Vector2 moveDir = { 0, 0 };
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_UP)) moveDir.y = -1;
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_DOWN)) moveDir.y = 1;
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_LEFT)) moveDir.x = -1;
    if (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_LEFT_FACE_RIGHT)) moveDir.x = 1;

    if (moveDir.x != 0 || moveDir.y != 0) {
        Vector2 currentPos = GetMousePosition();
        int bestIdx = -1;
        float bestDist = 9999999.0f;

        for (size_t i = 0; i < interactables.size(); i++) {
            Vector2 targetCenter = { interactables[i].x + interactables[i].width / 2.0f, interactables[i].y + interactables[i].height / 2.0f };
            Vector2 diff = Vector2Subtract(targetCenter, currentPos);

            float dot = (diff.x * moveDir.x + diff.y * moveDir.y);

            if (dot > 5.0f) {
                float proj = dot;
                float ortho = fabs(diff.x * moveDir.y - diff.y * moveDir.x);
                float score = proj + ortho * 4.0f;

                if (score < bestDist) {
                    bestDist = score;
                    bestIdx = i;
                }
            }
        }

        if (bestIdx != -1) {
            Vector2 center = { interactables[bestIdx].x + interactables[bestIdx].width / 2.0f, interactables[bestIdx].y + interactables[bestIdx].height / 2.0f };
            SetMousePosition((int)center.x, (int)center.y);
        }
    }
}

// ==========================================
// 汎用UIコンポーネントの描画
// ==========================================

void UI::OpenDetail(const ItemData& item) {
    focusingItem = item;
    showDetail = true;
    detailOpenTimer = 0.0f;
    AudioManager::PlaySE(SE_CLICK);
}

// ★修正: 第5引数に forceInteractable を追加
bool UI::DrawButton(Rectangle r, const char* label, Font font, Color col, bool forceInteractable) {
    UI::RegisterInteractable(r);

    // ★修正: forceInteractable が true の場合は、詳細画面(showDetail)が表示中でもロックしない
    bool locked = showDetail && !forceInteractable;
    bool clicked = false;
    bool hover = !locked && CheckCollisionPointRec(GetMousePosition(), r);

    Color drawCol = locked ? ColorBrightness(col, -0.4f) : col;
    if (hover) drawCol = ColorBrightness(col, 0.2f);

    DrawRectangleRec(r, drawCol);
    DrawRectangleLinesEx(r, 2, locked ? GRAY : RAYWHITE);

    Vector2 tSize = MeasureTextEx(font, label, 18, 1);
    DrawTextEx(font, label, { r.x + r.width / 2 - tSize.x / 2, r.y + r.height / 2 - tSize.y / 2 }, 18, 1, locked ? LIGHTGRAY : WHITE);

    bool clickInput = IsMouseButtonPressed(0) || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN);
    if (hover && clickInput) {
        clicked = true;
        AudioManager::PlaySE(SE_CLICK);
    }
    return clicked;
}

// =============================================================================
// アイテム詳細ウィンドウ（三回り拡大 ＆ 文字二回り大型化版）
// =============================================================================
void UI::DrawDetailWindow(Font font, Player& p) {
    if (!showDetail) return;
    detailOpenTimer += GetFrameTime();

    int sw = GetScreenWidth();
    int sh = GetScreenHeight();

    DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.75f));

    // ★ ウィンドウサイズを幅530px、高さ630pxに拡大！
    int w = 530; int h = 630;
    int x = (sw - w) / 2; int y = (sh - h) / 2;
    DrawRectangle(x, y, w, h, Fade(DARKBLUE, 0.95f));
    DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 3, GOLD);

    // アイテム名（28pxに大型化）
    Color rarityColor = Player::GetItemRarityColor(focusingItem);
    DrawTextEx(font, Player::GetFullItemName(focusingItem).c_str(), { (float)x + 30, (float)y + 25 }, 28, 1, rarityColor);

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
    DrawTextEx(font, typeStr.c_str(), { (float)x + 30, (float)y + 64 }, 20, 1, LIGHTGRAY);

    // 性能比較（Diff）の計算
    ItemData curEquipped;
    curEquipped.id = -1;
    bool isCurrentlyEquippedThis = false;

    if (focusingItem.type == "EQUIP") {
        curEquipped = p.equippedData[p.activeSlot];
        if (curEquipped.id == focusingItem.id && curEquipped.modifierId == focusingItem.modifierId && curEquipped.element == focusingItem.element) {
            isCurrentlyEquippedThis = true;
        }
    }
    else if (focusingItem.type == "ARMOR") {
        int sub = focusingItem.weaponSubtype;
        if (sub >= 0 && sub < 5) {
            curEquipped = p.equippedArmor[sub];
            if (curEquipped.id == focusingItem.id && curEquipped.modifierId == focusingItem.modifierId && curEquipped.element == focusingItem.element) {
                isCurrentlyEquippedThis = true;
            }
        }
    }

    if (isCurrentlyEquippedThis) {
        DrawTextEx(font, T("EQUIPPED_NOW", "[Equipped]").c_str(), { (float)x + w - 170, (float)y + 64 }, 20, 1, GREEN);
    }

    float curAtk = 0, curDef = 0, curHp = 0, curSpd = 0;
    if (curEquipped.id != -1) {
        Modifier curMod = DataManager::GetModifier(curEquipped.modifierId);
        curAtk = curEquipped.atkBonus + curMod.atk;
        curDef = curEquipped.defBonus + curMod.def;
        curHp = curEquipped.hpBonus + curMod.hp;
        curSpd = curEquipped.speedBonus + curMod.spd;
    }

    Modifier mod = DataManager::GetModifier(focusingItem.modifierId);
    float totalAtk = focusingItem.atkBonus + mod.atk;
    float totalDef = focusingItem.defBonus + mod.def;
    float totalHp = focusingItem.hpBonus + mod.hp;
    float totalSpd = focusingItem.speedBonus + mod.spd;

    float diffAtk = totalAtk - curAtk;
    float diffDef = totalDef - curDef;
    float diffHp = totalHp - curHp;
    float diffSpd = totalSpd - curSpd;

    // 差分テキスト描画
    auto drawDiff = [&](float diff, int drawY, bool isFloatVal = true) {
        if (focusingItem.type != "EQUIP" && focusingItem.type != "ARMOR") return;
        if (isCurrentlyEquippedThis) return;

        std::string diffStr = "";
        Color diffCol = LIGHTGRAY;
        if (diff > 0.001f) {
            diffStr = isFloatVal ? TextFormat("(+%.1f)", diff) : TextFormat("(+%.0f)", diff);
            diffCol = GREEN;
        }
        else if (diff < -0.001f) {
            diffStr = isFloatVal ? TextFormat("(%.1f)", diff) : TextFormat("(%.0f)", diff);
            diffCol = RED;
        }
        else if (curEquipped.id != -1) {
            diffStr = "(+0)";
        }

        if (!diffStr.empty()) {
            DrawTextEx(font, diffStr.c_str(), { (float)x + 360, (float)drawY }, 20, 1, diffCol);
        }
        };

    int statsY = y + 105; int lineH = 34; // 行間を34pxに拡大
    if (totalAtk != 0 || (curEquipped.id != -1 && curAtk != 0)) {
        DrawTextEx(font, TextFormat(T("STAT_ATK", "ATK : %+.1f").c_str(), totalAtk), { (float)x + 40, (float)statsY }, 22, 1, RED);
        if (mod.atk != 0) DrawTextEx(font, TextFormat("(Mod %+.1f)", mod.atk), { (float)x + 230, (float)statsY }, 18, 1, ORANGE);
        drawDiff(diffAtk, statsY);
        statsY += lineH;
    }
    if (totalDef != 0 || (curEquipped.id != -1 && curDef != 0)) {
        DrawTextEx(font, TextFormat(T("STAT_DEF", "DEF : %+.1f").c_str(), totalDef), { (float)x + 40, (float)statsY }, 22, 1, BLUE);
        if (mod.def != 0) DrawTextEx(font, TextFormat("(Mod %+.1f)", mod.def), { (float)x + 230, (float)statsY }, 18, 1, ORANGE);
        drawDiff(diffDef, statsY);
        statsY += lineH;
    }
    if (totalHp != 0 || (curEquipped.id != -1 && curHp != 0)) {
        DrawTextEx(font, TextFormat(T("STAT_HP", "HP : %+.0f").c_str(), totalHp), { (float)x + 40, (float)statsY }, 22, 1, GREEN);
        if (mod.hp != 0) DrawTextEx(font, TextFormat("(Mod %+.0f)", mod.hp), { (float)x + 230, (float)statsY }, 18, 1, ORANGE);
        drawDiff(diffHp, statsY, false);
        statsY += lineH;
    }
    if (totalSpd != 0 || (curEquipped.id != -1 && curSpd != 0)) {
        DrawTextEx(font, TextFormat(T("STAT_SPD", "SPD : %+.2f").c_str(), totalSpd), { (float)x + 40, (float)statsY }, 22, 1, SKYBLUE);
        if (mod.spd != 0) DrawTextEx(font, TextFormat("(Mod %+.2f)", mod.spd), { (float)x + 230, (float)statsY }, 18, 1, ORANGE);
        drawDiff(diffSpd, statsY);
        statsY += lineH;
    }
    if (focusingItem.heal > 0) {
        DrawTextEx(font, TextFormat(T("STAT_HEAL", "Heal : %.0f").c_str(), focusingItem.heal), { (float)x + 40, (float)statsY }, 22, 1, PINK);
        statsY += lineH;
    }

    // 1. モディファイア枠（幅を広げてゆったり表示）
    if (mod.id != 0) {
        statsY += 10;
        DrawRectangleLines(x + 25, statsY - 5, w - 50, 62, ORANGE);
        DrawTextEx(font, T("MODIFIER_TITLE", "Modifier:").c_str(), { (float)x + 35, (float)statsY }, 16, 1, ORANGE);
        DrawTextEx(font, mod.name.c_str(), { (float)x + 55, (float)statsY + 24 }, 22, 1, YELLOW);
        statsY += 68;
    }

    // 2. 属性表示枠（解説文もゆったり収まる幅）
    if (focusingItem.element != ELEM_NONE) {
        statsY += 10;
        Color elemCol = Player::GetElementColor(focusingItem.element);
        std::string elemTitle = "";
        std::string elemDesc = "";

        switch (focusingItem.element) {
        case ELEM_HELLFIRE:
            elemTitle = T("ELEM_NAME_HELLFIRE", "[Hellfire Element]");
            elemDesc = (focusingItem.type == "ARMOR") ? T("ELEM_DESC_ARMOR", "Resist: -15% Damage") : T("ELEM_DESC_HELLFIRE_WPN", "Strong vs Rot (1.4x) / Weak vs Soul");
            break;
        case ELEM_ROT:
            elemTitle = T("ELEM_NAME_ROT", "[Rot Element]");
            elemDesc = (focusingItem.type == "ARMOR") ? T("ELEM_DESC_ARMOR", "Resist: -15% Damage") : T("ELEM_DESC_ROT_WPN", "Strong vs Soul (1.4x) / Weak vs Hellfire");
            break;
        case ELEM_SOUL:
            elemTitle = T("ELEM_NAME_SOUL", "[Soul Element]");
            elemDesc = (focusingItem.type == "ARMOR") ? T("ELEM_DESC_ARMOR", "Resist: -15% Damage") : T("ELEM_DESC_SOUL_WPN", "Strong vs Hellfire (1.4x) / Weak vs Rot");
            break;
        case ELEM_ABYSS:
            elemTitle = T("ELEM_NAME_ABYSS", "[Abyss Element]");
            elemDesc = (focusingItem.type == "ARMOR") ? T("ELEM_DESC_ARMOR", "Resist: -15% Damage") : T("ELEM_DESC_ABYSS_WPN", "Strong vs All Elements (1.25x)");
            break;
        }

        DrawRectangleLines(x + 25, statsY - 5, w - 50, 66, elemCol);
        DrawTextEx(font, elemTitle.c_str(), { (float)x + 35, (float)statsY }, 20, 1, elemCol);
        DrawTextEx(font, elemDesc.c_str(), { (float)x + 40, (float)statsY + 28 }, 16, 1, WHITE);
        statsY += 72;
    }

    // 閉じるボタン（幅180pxに拡大）
    Rectangle closeBtn = { (float)x + w / 2 - 90, (float)y + h - 65, 180, 48 };
    bool inputEnabled = (detailOpenTimer >= 0.3f);
    if (UI::DrawButton(closeBtn, T("CLOSE", "Close").c_str(), font, inputEnabled ? MAROON : Fade(MAROON, 0.5f), true)) {
        if (inputEnabled) {
            showDetail = false;
        }
    }
}

int UI::DrawPrompt(const char* label, int sw, int sh, Font font) {
    std::string msg = T(label, label);
    int bw = 450, bh = 180;
    int bx = sw / 2 - bw / 2, by = sh / 2 - bh / 2;

    DrawRectangle(bx, by, bw, bh, Fade(BLACK, 0.9f));
    DrawRectangleLines(bx, by, bw, bh, GOLD);

    Vector2 tS = MeasureTextEx(font, msg.c_str(), 24, 1);
    DrawTextEx(font, msg.c_str(), { (float)sw / 2 - tS.x / 2, (float)by + 40 }, 24, 1, WHITE);

    Rectangle bY = { (float)sw / 2 - 140, (float)by + 100, 120, 50 };
    Rectangle bN = { (float)sw / 2 + 20, (float)by + 100, 120, 50 };

    int res = 0;
    if (UI::DrawButton(bY, T("YES", "YES").c_str(), font, GREEN)) res = 1;
    if (UI::DrawButton(bN, T("NO", "NO").c_str(), font, RED)) res = 2;
    return res;
}


// ==========================================
// 各種メイン画面の描画
// ==========================================

// タイトル画面(セーブデータ選択画面)
int UI::DrawTitleScreen(Font font) {
    int sw = GetScreenWidth(); int sh = GetScreenHeight();

    if (DataManager::titleBg.id != 0) {
        Rectangle source = { 0.0f, 0.0f, (float)DataManager::titleBg.width, (float)DataManager::titleBg.height };
        Rectangle dest = { 0.0f, 0.0f, (float)sw, (float)sh };
        Vector2 origin = { 0.0f, 0.0f };
        DrawTexturePro(DataManager::titleBg, source, dest, origin, 0.0f, WHITE);
    }
    else {
        // 画像がない場合はグラデーション背景
        DrawRectangleGradientV(0, 0, sw, sh, DARKBLUE, BLACK);
        const char* title = "3D Hack & Slash";
        Vector2 tSize = MeasureTextEx(font, title, 60, 2);
        DrawTextEx(font, title, { (float)(sw - tSize.x) / 2, 100.0f }, 60, 2, GOLD);
    }

    // セーブデータ削除の確認ダイアログ
    if (deleteConfirmSlot > 0) {
        DrawRectangle(0, 0, sw, sh, Fade(BLACK, 0.8f));
        int w = 500, h = 250; int x = (sw - w) / 2, y = (sh - h) / 2;
        DrawRectangle(x, y, w, h, DARKGRAY);
        DrawRectangleLinesEx({ (float)x, (float)y, (float)w, (float)h }, 3, RED);

        DrawTextEx(font, TextFormat(T("DELETE_CONFIRM", "Delete Slot %d ?").c_str(), deleteConfirmSlot), { (float)x + 40, (float)y + 60 }, 24, 1, WHITE);
        DrawTextEx(font, T("CANNOT_UNDONE", "Cannot be undone.").c_str(), { (float)x + 80, (float)y + 100 }, 20, 1, RED);

        if (DrawButton({ (float)x + 50, (float)y + 160, 150, 50 }, T("DELETE", "Delete").c_str(), font, RED)) {
            DataManager::DeleteSave(deleteConfirmSlot); deleteConfirmSlot = 0;
        }
        if (DrawButton({ (float)x + 300, (float)y + 160, 150, 50 }, T("CANCEL", "Cancel").c_str(), font, GRAY)) {
            deleteConfirmSlot = 0;
        }
        return 0; // ダイアログ展開中はゲームを開始しない
    }

    int selectedSlot = 0; // 開始スロット
    // スロット1〜3のボタンを描画
    for (int i = 1; i <= 3; i++) {
        SaveHeader h = DataManager::GetSaveHeader(i); // ヘッダのみを読み込んで状態を確認
        float y = 350.0f + (float)(i - 1) * 100.0f;
        Rectangle r = { (float)sw / 2 - 200, y, 400.0f, 80.0f };
        std::string label; Color c;

        if (h.exists) {
            if (h.isPortfolioMode) {
                label = TextFormat(T("SLOT_RUSH", "Slot %d:[RUSH] Lv.%d").c_str(), i, h.playerLevel);
                c = Fade(GOLD, 0.8f);
            }
            else {
                label = TextFormat(T("SLOT_DATA", "Slot %d: Lv.%d  Floor %d").c_str(), i, h.playerLevel, h.floor);
                c = Fade(DARKGREEN, 0.8f);
            }
        }
        else {
            label = TextFormat(T("SLOT_EMPTY", "Slot %d: (NO DATA)").c_str(), i);
            c = Fade(DARKGRAY, 0.8f);
        }

        if (DrawButton(r, label.c_str(), font, c)) { selectedSlot = i; }

        // データが存在する場合は、横に「×(削除)」ボタンを配置
        if (h.exists) {
            Rectangle delBtn = { r.x + r.width + 20, r.y + 20, 60, 40 };
            if (DrawButton(delBtn, "X", font, Fade(MAROON, 0.8f))) { deleteConfirmSlot = i; }
        }
    }

    // 特殊モードへの突入ボタン
    if (DrawButton({ 20, (float)sh - 110, 340, 40 }, T("PORTFOLIO_MODE", "Portfolio: 3-Floor Rush").c_str(), font, Fade(ORANGE, 0.8f))) return 888;
    if (DrawButton({ 20, (float)sh - 60, 200, 40 }, T("DEBUG_ROOM", "Debug Room").c_str(), font, Fade(PURPLE, 0.8f))) return 999;

    return selectedSlot;
}

// ゲームプレイ中のUI(HUD)描画
// =============================================================================
// ゲームプレイ中のUI(HUD)描画（文字・アイコン大型化＆視認性強化版）
// =============================================================================
void UI::DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, int dungeonId, bool debug, Font font) {
    int sw = GetScreenWidth(); int sh = GetScreenHeight();

    // --- 1. 左上: 現在の階層やステージ名 ---
    std::string floorText;
    if (floor > 1000) { floorText = TextFormat(T("STAGE", "STAGE %d").c_str(), floor - 1000); }
    else if (floor == 0) { floorText = T("HOME", "HOME"); }
    else {
        std::string label = T("FLOOR", "Floor");
        std::string dName = T("DUNGEON_1", "Dungeon 1");
        if (dungeonId == 1) dName = T("DUNGEON_2", "Dungeon 2");
        else if (dungeonId == 2) dName = T("DUNGEON_3", "Abyss");
        floorText = TextFormat("%s: %s %d", dName.c_str(), label.c_str(), floor);
    }
    Vector2 fSize = MeasureTextEx(font, floorText.c_str(), 26, 1);
    DrawRectangle(20, 20, (int)fSize.x + 35, 45, Fade(BLACK, 0.7f));
    DrawRectangleLines(20, 20, (int)fSize.x + 35, 45, Fade(GRAY, 0.6f));
    DrawTextEx(font, floorText.c_str(), { 35, 29 }, 26, 1, WHITE);

    // --- 2. 左側: クエストの進捗状況（文字を一回り拡大） ---
    if (!p.activeQuests.empty()) {
        int questY = 80; int questX = 20;
        DrawTextEx(font, T("QUESTS", "[Active Quests]").c_str(), { (float)questX, (float)questY }, 20, 1, GOLD);
        questY += 26;
        for (const auto& q : p.activeQuests) {
            QuestData qData = DataManager::GetQuestData(q.questId);
            if (qData.id != -1) {
                std::string progressStr; Color textColor = WHITE;
                if (q.isCompleted) { progressStr = T("DONE", "[Done]"); textColor = GREEN; }
                else {
                    if (qData.type == QUEST_HUNT) { progressStr = TextFormat(" [%d/%d]", q.currentCount, qData.targetCount); }
                    else if (qData.type == QUEST_GATHER) {
                        int currentInvCount = 0;
                        for (const auto& it : p.inventoryItems) { if (it.id == qData.targetId) currentInvCount += it.count; }
                        progressStr = TextFormat(" [%d/%d]", currentInvCount, qData.targetCount);
                        if (currentInvCount >= qData.targetCount) { progressStr += T("DONE", " [Done]"); textColor = GREEN; }
                    }
                }
                std::string displayText = qData.title + progressStr;
                Vector2 tSize = MeasureTextEx(font, displayText.c_str(), 18, 1);
                DrawRectangle(questX - 5, questY - 2, (int)tSize.x + 10, 22, Fade(BLACK, 0.6f));
                DrawTextEx(font, displayText.c_str(), { (float)questX, (float)questY }, 18, 1, textColor);
                questY += 26;
            }
        }
    }

    // --- 3. 右上: ミニマップ ---
    int mapSize = 210; int mapX = sw - mapSize - 20; int mapY = 20;
    DrawRectangle(mapX, mapY, mapSize, mapSize, Fade(BLACK, 0.65f));
    DrawRectangleLines(mapX, mapY, mapSize, mapSize, GRAY);

    BeginScissorMode(mapX, mapY, mapSize, mapSize);
    float sc = 8.5f;
    float offX = mapX + mapSize / 2.0f - (p.position.x / TILE_SIZE) * sc;
    float offY = mapY + mapSize / 2.0f - (p.position.z / TILE_SIZE) * sc;

    for (int y = 0; y < d.currentHeight; y++) {
        for (int x = 0; x < d.currentWidth; x++) {
            if (d.IsDiscovered((float)x * TILE_SIZE, (float)y * TILE_SIZE)) {
                Color tileCol = d.IsWall((float)x * TILE_SIZE, (float)y * TILE_SIZE) ? GRAY : DARKGREEN;
                DrawRectangle(offX + x * sc, offY + y * sc, sc - 1, sc - 1, tileCol);
            }
        }
    }

    auto drawMapIcon = [&](Vector3 pos, Color col) {
        if (pos.x != -999 && d.IsDiscovered(pos.x, pos.z)) {
            DrawRectangle(offX + (pos.x / TILE_SIZE) * sc, offY + (pos.z / TILE_SIZE) * sc, sc, sc, col);
        }
        };

    drawMapIcon(d.stairsDownPos, GOLD); drawMapIcon(d.stairsUpPos, SKYBLUE); drawMapIcon(d.portalPos, PURPLE);
    if (d.isHome) {
        for (int i = 0; i < 3; i++) drawMapIcon(d.dungeonEntrances[i], GOLD);
        drawMapIcon(d.storageBoxPos, BROWN); drawMapIcon(d.reforgeStationPos, PURPLE);
        drawMapIcon(d.craftStationPos, ORANGE); drawMapIcon(d.questBoardPos, BEIGE);
    }
    else {
        drawMapIcon(d.healStationPos, PINK); drawMapIcon(d.bossSpawnPos, MAGENTA);
    }
    DrawCircle(mapX + mapSize / 2, mapY + mapSize / 2, 5, RED);
    EndScissorMode();

    // --- 4. 敵のHPバー描画（右側HUD ＆ 頭上バー大型化） ---
    int listCount = 0;
    for (auto& e : enemies) {
        if (e.hudTimer > 0) {
            int yPos = mapY + mapSize + 20 + listCount * 56;
            DrawRectangle(sw - 250, yPos, 230, 50, Fade(BLACK, 0.75f));
            DrawRectangleLines(sw - 250, yPos, 230, 50, Fade(GRAY, 0.5f));

            std::string elemTag = "";
            Color elemTagCol = WHITE;
            if (e.data.element != ELEM_NONE) {
                elemTag = TextFormat("[%s]", Player::GetElementName(e.data.element).c_str());
                elemTagCol = Player::GetElementColor(e.data.element);
            }

            DrawTextEx(font, TextFormat("Lv.%d %s", e.level, e.data.name.c_str()), { (float)sw - 240, (float)yPos + 6 }, 18, 1, WHITE);
            if (!elemTag.empty()) {
                DrawTextEx(font, elemTag.c_str(), { (float)sw - 90, (float)yPos + 7 }, 15, 1, elemTagCol);
            }

            DrawRectangle(sw - 240, yPos + 30, 210, 12, DARKGRAY);
            float hpRate = fmaxf(0.0f, e.hp / e.maxHp);
            DrawRectangle(sw - 240, yPos + 30, (int)(210.0f * hpRate), 12, RED);

            listCount++;
            if (listCount >= 5) break;
        }

        // ★ 3D空間の頭上バー（文字とバーの厚みを大幅拡大！）
        if (debug || d.IsDiscovered((float)e.position.x, (float)e.position.z)) {
            Vector2 s = GetWorldToScreen(e.position, cam);
            if (s.x > 0 && s.y > 0 && s.x < sw && s.y < sh) {
                std::string txt = TextFormat("Lv.%d %s", e.level, e.data.name.c_str());
                if (e.data.element != ELEM_NONE) {
                    txt += TextFormat(" [%s]", Player::GetElementName(e.data.element).c_str());
                }

                Color nameCol = (e.data.element != ELEM_NONE) ? Player::GetElementColor(e.data.element) : WHITE;
                Vector2 tSize = MeasureTextEx(font, txt.c_str(), 20, 1);

                // 頭上文字（20pxでクッキリ表示）
                DrawTextEx(font, txt.c_str(), { s.x - tSize.x / 2 + 1, s.y - 51 }, 20, 1, Fade(BLACK, 0.85f));
                DrawTextEx(font, txt.c_str(), { s.x - tSize.x / 2, s.y - 52 }, 20, 1, nameCol);

                // 頭上バー（幅60px、厚み8pxに拡大）
                DrawRectangle((int)s.x - 30, (int)s.y - 28, 60, 8, DARKGRAY);
                DrawRectangleLines((int)s.x - 30, (int)s.y - 28, 60, 8, BLACK);
                float headHpRate = fmaxf(0.0f, e.hp / e.maxHp);
                DrawRectangle((int)s.x - 30, (int)s.y - 28, (int)(60.0f * headHpRate), 8, RED);
            }
        }
    }

    // --- 5. 左下: プレイヤーステータス（枠と文字の大型化） ---
    DrawRectangle(10, sh - 145, 360, 135, Fade(BLACK, 0.7f));
    DrawRectangleLines(10, sh - 145, 360, 135, Fade(GRAY, 0.5f));

    // レベル ＆ EXP（20px）
    DrawTextEx(font, TextFormat(T("HUD_LV_EXP", "Lv: %d   EXP: %d/%d").c_str(), p.level, p.exp, p.expToNext), { 25, (float)sh - 138 }, 20, 1, SKYBLUE);

    // HPバー（厚み22pxに拡大）
    DrawRectangle(25, sh - 108, 330, 22, DARKGRAY);
    DrawRectangle(25, sh - 108, (int)(330 * (fmaxf(0.0f, p.hp) / p.maxHp)), 22, GREEN);
    DrawRectangleLines(25, sh - 108, 330, 22, BLACK);
    DrawTextEx(font, TextFormat(T("HUD_HP", "HP: %.0f / %.0f").c_str(), p.hp, p.maxHp), { 35, (float)sh - 106 }, 17, 1, WHITE);

    // 攻撃力・防御力（20px）
    DrawTextEx(font, TextFormat(T("HUD_STATS", "ATK: %.1f   DEF: %.1f").c_str(), p.attackPower, p.defense), { 25, (float)sh - 78 }, 20, 1, WHITE);

    // 所持金・スキルポイント（20px）
    DrawTextEx(font, TextFormat(T("HUD_GOLD_SP", "Gold: %d G   SP: %d").c_str(), p.gold, p.skillPoints), { 25, (float)sh - 48 }, 20, 1, YELLOW);

    // --- 6. 右下: スキルアイコン（40px → 52px に大型化！） ---
    int iconSize = 52;
    int startX = sw - 370;
    int startY = sh - 75;

    struct SkillIcon { SkillType type; const char* labelKey; std::string key; std::string padKey; };
    SkillIcon icons[] = {
        { SKILL_ACTIVE_SMASH, "SMASH", getKeyStr(DataManager::keyConfig.smash), UI::GetPadBtnStr(DataManager::keyConfig.padSmash) },
        { SKILL_ACTIVE_KONGO, "KONGO", getKeyStr(DataManager::keyConfig.kongo), UI::GetPadBtnStr(DataManager::keyConfig.padKongo) },
        { SKILL_ACTIVE_ZOUKYOU, "ATKUP", getKeyStr(DataManager::keyConfig.zoukyou), UI::GetPadBtnStr(DataManager::keyConfig.padZoukyou) },
        { SKILL_ACTIVE_STEALTH, "HIDE", getKeyStr(DataManager::keyConfig.stealth), UI::GetPadBtnStr(DataManager::keyConfig.padStealth) },
        { SKILL_ACTIVE_HEAL, "HEAL", getKeyStr(DataManager::keyConfig.heal), UI::GetPadBtnStr(DataManager::keyConfig.padHeal) },
        { SKILL_ACTIVE_DASH, "DASH", getKeyStr(DataManager::keyConfig.dash), UI::GetPadBtnStr(DataManager::keyConfig.padDash) }
    };

    bool padActive = IsGamepadAvailable(0);

    for (int i = 0; i < 6; i++) {
        int x = startX + i * (iconSize + 8);
        bool unlocked = p.IsSkillUnlocked(icons[i].type);
        Color baseCol = unlocked ? DARKBLUE : DARKGRAY;

        DrawRectangle(x, startY, iconSize, iconSize, baseCol);
        DrawRectangleLines(x, startY, iconSize, iconSize, RAYWHITE);

        // キー操作ガイド（左上に14pxで表示）
        std::string displayKey = padActive ? icons[i].padKey : icons[i].key;
        DrawTextEx(font, displayKey.c_str(), { (float)x + 4, (float)startY + 3 }, 14, 1, YELLOW);

        if (unlocked) {
            float cd = p.GetSkillCooldown(icons[i].type);
            float maxCd = p.GetSkillMaxCooldown(icons[i].type);

            if (cd > 0) {
                float ratio = cd / maxCd;
                DrawRectangle(x, startY + (int)((float)iconSize * (1.0f - ratio)), iconSize, (int)((float)iconSize * ratio), Fade(RED, 0.7f));
                DrawTextEx(font, TextFormat("%.1f", cd), { (float)x + 6, (float)startY + 20 }, 18, 1, YELLOW);
            }
            else {
                // スキル名（下部に13pxで表示）
                std::string label = T(icons[i].labelKey, icons[i].labelKey);
                DrawTextEx(font, label.c_str(), { (float)x + 4, (float)startY + 32 }, 13, 1, GREEN);
            }
        }
        else {
            DrawTextEx(font, T("SKILL_LOCKED", "LOCK").c_str(), { (float)x + 8, (float)startY + 22 }, 13, 1, GRAY);
        }
    }
}

// =============================================================================
// 足元に落ちているアイテムの名前を3D連動で表示（性能比較三角アイコン付き）
// =============================================================================
void UI::DrawNearbyItems(Player& p, std::vector<DroppedItem>& di, Dungeon& d, Camera3D& cam, Font font) {
    for (auto& item : di) {
        if (!d.IsDiscovered(item.pos.x, item.pos.z)) continue;

        float dist = Vector3Distance(p.position, item.pos);
        if (dist < 5.0f) { // プレイヤーから5m以内のみ表示
            Vector2 s = GetWorldToScreen(item.pos, cam);
            if (s.x > 0 && s.y > 0) {
                // ★ 落ちているアイテムの性能比較判定
                int comp = Player::CompareWithEquipped(item.data, p);
                float triX = s.x - 24.0f;
                float triY = s.y - 12.0f;

                // 強いなら緑▲、弱いなら赤▼を確実に描画
                if (comp == 1) {
                    Vector2 pTop = { triX, triY - 6.0f };
                    Vector2 pRight = { triX + 6.0f, triY + 6.0f };
                    Vector2 pLeft = { triX - 6.0f, triY + 6.0f };
                    DrawTriangle(pTop, pRight, pLeft, LIME);
                    DrawTriangle(pTop, pLeft, pRight, LIME);
                }
                else if (comp == -1) {
                    Vector2 pBottom = { triX, triY + 6.0f };
                    Vector2 pLeft = { triX - 6.0f, triY - 6.0f };
                    Vector2 pRight = { triX + 6.0f, triY - 6.0f };
                    DrawTriangle(pBottom, pLeft, pRight, RED);
                    DrawTriangle(pBottom, pRight, pLeft, RED);
                }

                DrawTextEx(font, Player::GetFullItemName(item.data).c_str(), { s.x - 12.0f, s.y - 20.0f }, 16, 1, Player::GetItemRarityColor(item.data));
            }
        }
    }
}

// 画面左下に蓄積するシステムログの追加
void UI::AddSystemLog(const std::string& text, Color color) {
    SystemLogMessage msg; msg.text = text; msg.color = color; msg.lifeTime = 5.0f; msg.maxLifeTime = 5.0f;
    systemLogs.push_back(msg);
    if (systemLogs.size() > 10) { systemLogs.erase(systemLogs.begin()); } // 最大10件まで保持
}

void UI::UpdateSystemLogs(float deltaTime) {
    for (auto it = systemLogs.begin(); it != systemLogs.end(); ) {
        it->lifeTime -= deltaTime;
        if (it->lifeTime <= 0.0f) { it = systemLogs.erase(it); }
        else { ++it; }
    }
}

// =============================================================================
// 戦闘・システムログの描画
// =============================================================================
void UI::DrawSystemLogs(Font font) {
    if (systemLogs.empty()) return;

    // ステータス枠（sh - 145）のすぐ上に配置
    int startY = GetScreenHeight() - 170;
    int startX = 20;
    int fontSize = 21; 
    int lineSpacing = 28;

    int displayCount = (int)systemLogs.size();
    if (displayCount > 6) displayCount = 6;

    for (int i = 0; i < displayCount; ++i) {
        int logIdx = (int)systemLogs.size() - 1 - i;
        auto& log = systemLogs[logIdx];

        float alpha = 1.0f;
        if (log.lifeTime < 1.0f) alpha = log.lifeTime;
        if (alpha < 0.0f) alpha = 0.0f;

        Color textColor = log.color;
        textColor.a = (unsigned char)(255 * alpha);

        int drawY = startY - (i * lineSpacing);
        Vector2 tSize = MeasureTextEx(font, log.text.c_str(), (float)fontSize, 1);

        Color bgCol = Fade(BLACK, 0.7f * alpha);
        DrawRectangle(startX - 6, drawY - 2, (int)tSize.x + 12, fontSize + 5, bgCol);
        DrawRectangleLines(startX - 6, drawY - 2, (int)tSize.x + 12, fontSize + 5, Fade(DARKGRAY, 0.5f * alpha));

        DrawTextEx(font, log.text.c_str(), { (float)startX + 1, (float)drawY + 1 }, (float)fontSize, 1, Fade(BLACK, alpha));
        DrawTextEx(font, log.text.c_str(), { (float)startX, (float)drawY }, (float)fontSize, 1, textColor);
    }


}

void UI::DrawLogs(std::vector<GameLog>& logs, Player& p, Camera3D& cam, Font font) {
    Vector3 headPos = Vector3Add(p.position, { 0, 2.2f, 0 });
    Vector2 screenPos = GetWorldToScreen(headPos, cam);
    if (screenPos.x < 0 || screenPos.y < 0 || screenPos.x > GetScreenWidth() || screenPos.y > GetScreenHeight()) return;

    for (int i = 0; i < (int)logs.size(); i++) {
        float a = fminf(1.0f, logs[i].life * 2.0f); // 消えかけでフェードアウト
        float moveUp = (4.0f - logs[i].life) * 12.0f; // 時間と共に上に昇る
        float yOffset = (i * 26.0f) + moveUp;

        Vector2 tSize = MeasureTextEx(font, logs[i].message.c_str(), 22, 1);
        Vector2 drawPos = { screenPos.x - tSize.x / 2.0f, screenPos.y - 45.0f - yOffset };

        // 影（黒文字）を描いてから本体を描画
        DrawTextEx(font, logs[i].message.c_str(), { drawPos.x + 1.5f, drawPos.y + 1.5f }, 22, 1, Fade(BLACK, a * 0.9f));
        DrawTextEx(font, logs[i].message.c_str(), drawPos, 22, 1, Fade(logs[i].color, a));
    }
}