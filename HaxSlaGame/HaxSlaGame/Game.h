#pragma once
#include "raylib.h"
#include "Dungeon.h"
#include "Player.h"
#include "Enemy.h"
#include "EffectManager.h"
#include "UI.h"
#include <vector>

class Game {
public:
    Game();
    ~Game();
    void Run();

private:
    void InitGame();
    void Update();
    void Draw();
    void SpawnEnemies(int count);
    void NextFloor();
    void ReturnHome();

    int screenWidth = 1280;
    int screenHeight = 720;
    Font font;

    // ゲームオブジェクト
    Dungeon dungeon;
    Player* player; // ポインタ管理
    EffectManager fxManager;
    std::vector<Enemy> enemies;
    std::vector<DroppedItem> droppedItems;
    std::vector<GameLog> logs;
    std::vector<ItemData> storageItems;
    std::vector<ItemData> storageEquip;

    // ステート管理
    Camera3D camera;
    GameState state;
    int floor;
    bool debugMode;
    bool showMenu;
    bool showStorage;
    bool showPrompt;
    MenuTab currentTab;
    float sceneTimer;
};