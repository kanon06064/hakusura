#pragma once
#include "Definitions.h" 
#include "raylib.h"
#include <vector>

class Player;
class Enemy;
class Dungeon;
class Game;

class UI {
public:
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, bool debug, Font font);
    static void DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font);
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen);
    static void DrawCraftingMenu(class Player& p, Font font, bool& isOpen);

    static int DrawTitleScreen(Font font);
    static void DrawWarpMenu(void* gamePtr, int unlockedDungeon, const std::vector<int>& maxFloors, Font font, bool& isOpen);

    static int DrawPrompt(const char* label, int sw, int sh, Font font);
    static void DrawLogs(std::vector<GameLog>& logs, class Player& p, Camera3D& cam, Font font);
    static void DrawNearbyItems(class Player& p, std::vector<DroppedItem>& di, class Dungeon& d, Camera3D& cam, Font font);

    // ★ DrawButton を public に移動し、アクセス違反エラーを解消
    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol);

    static int itemPage, equipPage, debugPage, storageInvPage, storageBoxPage, itemSubTab;
    static int selectedDungeonTab; // ワープタブ用

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

    static void DrawDetailWindow(Font font);
    static void OpenDetail(const ItemData& item);
};