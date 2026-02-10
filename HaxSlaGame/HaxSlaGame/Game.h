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
    void WarpToFloor(int targetFloor);

private:
    void InitGame();
    void Update();
    void Draw();
    void SpawnEnemies(int count);
    void NextFloor();
    void ReturnHome();
    void ApplyDeathPenalty();
    void SaveCurrentSlot();
    void LoadAndStart(int slot);
    void NewGameAndStart(int slot);

    // 【修正】画面サイズとレンダーターゲット
    int windowWidth;
    int windowHeight;
    int gameWidth;
    int gameHeight;
    RenderTexture2D renderTarget;

    Font font;

    Dungeon dungeon;
    Player* player;
    EffectManager fxManager;
    std::vector<Enemy> enemies;
    std::vector<DroppedItem> droppedItems;
    std::vector<GameLog> logs;
    std::vector<ItemData> storageItems;
    std::vector<ItemData> storageEquip;

    Camera3D camera;
    GameState state;
    int floor;
    int maxReachedFloor;
    int currentSlot;

    bool debugMode;
    bool showMenu;
    bool showStorage;
    bool showReforgeMenu;
    bool showWarpMenu;
    bool showCraftMenu;
    bool showPrompt;
    MenuTab currentTab;
    float sceneTimer;

    float menuInputDelay;
    bool bossDefeated;
};