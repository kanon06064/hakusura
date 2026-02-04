#pragma once
#include "Definitions.h"
#include "raylib.h"
#include <vector>

// 前方宣言
class Player;
class Enemy;
class Dungeon;

class UI {
public:
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, bool debug, Font font);
    static void DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font);
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);
    static int DrawPrompt(const char* label, int sw, int sh, Font font);
    static void DrawLogs(std::vector<GameLog>& logs, Font font);
    static void DrawNearbyItems(class Player& p, std::vector<DroppedItem>& di, Camera3D& cam, Font font);

    // メンバ変数（ページ管理等）
    static int itemPage, equipPage, debugPage, storageInvPage, storageBoxPage, itemSubTab;
private:
    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol);
};