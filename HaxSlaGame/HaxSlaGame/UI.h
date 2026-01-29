#pragma once
#include "Definitions.h"

class Player; class Enemy; class Dungeon;

class UI {
public:
    static void DrawHUD(Player& p, std::vector<Enemy>& enemies, Dungeon& d, Camera3D& cam, int floor, bool debug);
    static void DrawMenu(Player& p, Dungeon& d, MenuTab& tab);
    static int DrawPrompt(const char* msg, int sw, int sh);
};