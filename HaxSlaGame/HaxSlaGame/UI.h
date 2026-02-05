#pragma once
#include "Definitions.h"
#include "raylib.h"
#include <vector>

class Player;
class Enemy;
class Dungeon;

class UI {
public:
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, bool debug, Font font);
    static void DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font);
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen);
    static int DrawPrompt(const char* label, int sw, int sh, Font font);
    static void DrawLogs(std::vector<GameLog>& logs, Font font);

    // ÅyèCê≥Åzà¯êîÇ… class Dungeon& d Çí«â¡
    static void DrawNearbyItems(class Player& p, std::vector<DroppedItem>& di, class Dungeon& d, Camera3D& cam, Font font);

    static int itemPage, equipPage, debugPage, storageInvPage, storageBoxPage, itemSubTab;
private:
    static int reforgeItemIdx;
    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol);
};