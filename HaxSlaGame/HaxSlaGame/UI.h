#pragma once
#include "Definitions.h"
#include "raylib.h"
#include <vector>

class Player;
class Enemy;
class Dungeon;

class UI {
public:
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, bool debug, Font font, int screenW, int screenH);
    static void DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font, int screenW, int screenH);
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip, int screenW, int screenH);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen, int screenW, int screenH);
    static void DrawCraftingMenu(class Player& p, Font font, bool& isOpen, int screenW, int screenH);

    static int DrawTitleScreen(Font font, int screenW, int screenH);
    static int DrawWarpMenu(int maxFloor, Font font, bool& isOpen, bool inputEnabled, int screenW, int screenH);

    // 【追加】詳細ウィンドウ描画関数
    static void DrawItemDetail(Font font, int screenW, int screenH);

    static int DrawPrompt(const char* label, int sw, int sh, Font font);
    static void DrawLogs(std::vector<GameLog>& logs, class Player& p, Camera3D& cam, Font font, int screenW, int screenH);
    static void DrawNearbyItems(class Player& p, std::vector<DroppedItem>& di, class Dungeon& d, Camera3D& cam, Font font);

    static int itemPage, equipPage, debugPage, storageInvPage, storageBoxPage, itemSubTab;
private:
    static int reforgeItemIdx;
    static int warpScroll;
    static int craftingScroll;
    static Vector2 skillOffset;

    // 【追加】詳細表示用ステート
    static bool showDetail;
    static ItemData detailItem;

    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol);
};