#pragma once
#include "Definitions.h" 
#include "raylib.h"
#include <vector>
#include <string>

// 画面左下に表示されるシステムログ（「〇〇を入手した」など）の構造体
struct SystemLogMessage {
    std::string text;
    Color color;
    float lifeTime;
    float maxLifeTime;
};

class Player;
class Enemy;
class Dungeon;
class Game;

class UI {
public:
    // --- 各種画面の描画関数 ---
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, int dungeonId, bool debug, Font font);
    static int DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font);

    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen);
    static void DrawCraftingMenu(class Player& p, Font font, bool& isOpen);
    static void DrawQuestMenu(class Player& p, Font font, bool& isOpen);

    static int DrawTitleScreen(Font font);
    static void DrawWarpMenu(void* gamePtr, int unlockedDungeon, const std::vector<int>& maxFloors, Font font, bool& isOpen);

    static int DrawPrompt(const char* label, int sw, int sh, Font font);

    static void DrawLogs(std::vector<GameLog>& logs, class Player& p, Camera3D& cam, Font font);
    static void DrawNearbyItems(class Player& p, std::vector<DroppedItem>& di, class Dungeon& d, Camera3D& cam, Font font);

    // ★修正: 第5引数に bool forceInteractable = false を追加
    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol, bool forceInteractable = false);

    // --- システムログ関連 ---
    static void AddSystemLog(const std::string& text, Color color = WHITE);
    static void UpdateSystemLogs(float deltaTime);
    static void DrawSystemLogs(Font font);

    static std::string GetPadBtnStr(int btn);

    // --- ゲームパッドのUIナビゲーション（スナップ移動）機構 ---
    static void ClearInteractables();
    static void RegisterInteractable(Rectangle r);
    static void UpdatePadNavigation();

    // --- UIのページ遷移・スクロール状態を保持する変数 ---
    static int itemPage, equipPage, storageInvPage, storageBoxPage, itemSubTab;
    static int selectedDungeonTab;
    static int questScroll;

    // --- アイテム詳細画面(サブウィンドウ)の管理 ---
    static bool showDetail;
    static ItemData focusingItem;
    static float detailOpenTimer;
    static int deleteConfirmSlot;

private:
    static int reforgeItemIdx;
    static int warpScroll;
    static int craftingScroll;
    static Vector2 skillOffset;
    static Vector2 mapOffset;

    static std::vector<SystemLogMessage> systemLogs;
    static std::vector<Rectangle> interactables;

    static void DrawDetailWindow(Font font);
    static void OpenDetail(const ItemData& item);
};