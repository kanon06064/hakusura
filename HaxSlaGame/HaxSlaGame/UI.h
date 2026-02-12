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
    static void DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font, int screenW, int screenH, bool inputEnabled);
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip, int screenW, int screenH, bool inputEnabled);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen, int screenW, int screenH, bool inputEnabled);
    static void DrawCraftingMenu(class Player& p, Font font, bool& isOpen, int screenW, int screenH, bool inputEnabled);

    static int DrawTitleScreen(Font font, int screenW, int screenH);
    static int DrawWarpMenu(int maxFloor, Font font, bool& isOpen, bool inputEnabled, int screenW, int screenH);

    static void DrawItemDetail(Font font, int screenW, int screenH);

    static int DrawPrompt(const char* label, int sw, int sh, Font font);
    static void DrawLogs(std::vector<GameLog>& logs, class Player& p, Camera3D& cam, Font font, int screenW, int screenH);

    // DrawNearbyItems を廃止し、DrawOverheadUI に統合
    static void DrawOverheadUI(class Player& p, std::vector<class Enemy>& enemies, std::vector<DroppedItem>& di, class Dungeon& d, Camera3D& cam, Font font, int screenW, int screenH);

    // 座標変換ヘルパー
    static Vector2 GetWorldToScreenScaled(Vector3 position, Camera3D camera, int width, int height);

    static int itemPage, equipPage, debugPage, storageInvPage, storageBoxPage, itemSubTab;

    static bool IsDetailOpen() { return showDetail; }

private:
    static int reforgeItemIdx;
    static int warpScroll;
    static int craftingScroll;
    static Vector2 skillOffset;
    static Vector2 mapOffset; // マップ移動用

    static bool showDetail;
    static ItemData detailItem;
    static double timeOnOpen;

    static int storageSelectedIndex;
    static bool storageIsDeposit;
    static int storageTransferCount;

    static int deleteTargetSlot; // 削除確認用

    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol, bool active = true);
};