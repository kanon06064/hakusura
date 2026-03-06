#pragma once
#include "Definitions.h"
#include "raylib.h"
#include <vector>

class Player;
class Enemy;
class Dungeon;

class UI {
public:
<<<<<<< HEAD
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, bool debug, Font font, int screenW, int screenH);
    static void DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font, int screenW, int screenH, bool inputEnabled);
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip, int screenW, int screenH, bool inputEnabled);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen, int screenW, int screenH, bool inputEnabled);
    static void DrawCraftingMenu(class Player& p, Font font, bool& isOpen, int screenW, int screenH, bool inputEnabled);
=======
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, bool debug, Font font);
    static void DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font);
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen);
    static void DrawCraftingMenu(class Player& p, Font font, bool& isOpen);
>>>>>>> sub

    static int DrawTitleScreen(Font font, int screenW, int screenH);
    static int DrawWarpMenu(int maxFloor, Font font, bool& isOpen, bool inputEnabled, int screenW, int screenH);

    static void DrawItemDetail(Font font, int screenW, int screenH);

    static int DrawPrompt(const char* label, int sw, int sh, Font font);
    static void DrawLogs(std::vector<GameLog>& logs, class Player& p, Camera3D& cam, Font font, int screenW, int screenH);

    // 【修正】DrawNearbyItems を削除し、DrawOverheadUI を宣言
    static void DrawOverheadUI(class Player& p, std::vector<class Enemy>& enemies, std::vector<DroppedItem>& di, class Dungeon& d, Camera3D& cam, Font font, int screenW, int screenH);

    // 座標変換ヘルパー
    static Vector2 GetWorldToScreenScaled(Vector3 position, Camera3D camera, int width, int height);
    static Ray GetMouseRayScaled(Vector2 mousePosition, Camera3D camera, int width, int height);

    static int itemPage, equipPage, debugPage, storageInvPage, storageBoxPage, itemSubTab;

<<<<<<< HEAD
    static bool IsDetailOpen() { return showDetail; }
=======
    // 詳細ウィンドウ管理用
    static bool showDetail;
    static ItemData focusingItem;
    static float detailOpenTimer;

    // 【追加】削除確認用
    static int deleteConfirmSlot;
>>>>>>> sub

private:
    static int reforgeItemIdx;
    static int warpScroll;
    static int craftingScroll;
    static Vector2 skillOffset;
    static Vector2 mapOffset;
<<<<<<< HEAD

    static bool showDetail;
    static ItemData detailItem;
    static double timeOnOpen;

    static int storageSelectedIndex;
    static bool storageIsDeposit;
    static int storageTransferCount;

    static int deleteTargetSlot;

    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol, bool active = true);
=======
    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol);

    static void DrawDetailWindow(Font font);
    static void OpenDetail(const ItemData& item);
>>>>>>> sub
};