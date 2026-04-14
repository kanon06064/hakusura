#pragma once
#include "raylib.h"
#include "Definitions.h"
#include "Dungeon.h"
#include "Player.h"
#include "Enemy.h"
#include "EffectManager.h"
#include "UI.h"
#include <vector>

// ★ ImGui インクルード
#include "imgui.h"
#include "rlImGui.h"

class Game {
public:
    Game();
    ~Game();
    void Run();

    // ★ワープ関数にダンジョンIDの引数を追加
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

    // ★ダンジョン進行度管理
    int currentDungeonId;   // 今いるダンジョン (0:最初の30階, 1:次の50階, 2:最後の100階)
    int unlockedDungeonId;  // どこまで解放されたか
    std::vector<int> maxFloors; // 各ダンジョンごとの最高到達階層

    int currentSlot;

    bool debugMode;
    bool showMenu;
    bool showStorage;
    bool showReforgeMenu;
    bool showWarpMenu;
    bool showCraftMenu;
    bool showQuestMenu; // ★追加：クエストボード表示フラグ
    bool showPrompt;
    MenuTab currentTab;
    float sceneTimer;

    bool bossDefeated;
    bool isPortfolioMode;
};