#pragma once
#include "raylib.h"
#include "Definitions.h"
#include "Dungeon.h"
#include "Player.h"
#include "Enemy.h"
#include "EffectManager.h"
#include "UI.h"
#include <vector>

#include "imgui.h"
#include "rlImGui.h"

class Game {
public:
    Game();
    ~Game();
    void Run();

    void WarpToFloor(int targetDungeon, int targetFloor);
    void StartDebugRoom();

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

    void InitDebugRoom();
    void UpdateDebugRoom();
    void DrawDebugRoom();

    void StartPortfolioMode();

    int screenWidth;
    int screenHeight;
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

    int currentDungeonId;
    int unlockedDungeonId;
    std::vector<int> maxFloors;

    int currentSlot;
    // ★ここがエラーの解消に必要な変数です
    int hoveredEntranceIndex;

    bool debugMode;
    bool showMenu;
    bool showStorage;
    bool showReforgeMenu;
    bool showWarpMenu;
    bool showCraftMenu;
    bool showQuestMenu;
    bool showPrompt;
    MenuTab currentTab;
    float sceneTimer;

    bool bossDefeated;
    bool isPortfolioMode;
};