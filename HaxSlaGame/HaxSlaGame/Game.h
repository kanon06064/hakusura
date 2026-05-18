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
    void Run(); // ゲームのメインループを実行する

    // ダンジョンの特定の階層へ直接ワープする（ポータル機能用）
    void WarpToFloor(int targetDungeon, int targetFloor);
    // デバッグルーム（敵の動きや攻撃をテストする専用部屋）を開始する
    void StartDebugRoom();

private:
    void InitGame();        // 新しいゲームを開始するための変数の初期化
    void Update();          // 毎フレームのロジック更新（入力、移動、当たり判定など）
    void Draw();            // 毎フレームの描画処理（3Dモデル、UI、デバッグメニュー）
    void SpawnEnemies(int count); // ダンジョンに敵をスポーンさせる
    void NextFloor();       // 次の階層へ進む（階段を降りた時の処理）
    void ReturnHome();      // 拠点（ホーム）へ帰還する

    void ApplyDeathPenalty();     // プレイヤー死亡時のペナルティ（アイテムロストと帰還）
    void SaveCurrentSlot();       // 現在のプレイデータをセーブする
    void LoadAndStart(int slot);  // 選択したスロットからセーブデータをロードして開始する
    void NewGameAndStart(int slot); // 新しいスロットで最初から開始する

    void InitDebugRoom();
    void UpdateDebugRoom();
    void DrawDebugRoom();

    void StartPortfolioMode();    // ポートフォリオ用の「3層ボスラッシュモード」を開始する

    int screenWidth;
    int screenHeight;
    Font font; // 日本語対応のフォント

    // --- ゲームを構成する主要なインスタンス ---
    Dungeon dungeon;
    Player* player;
    EffectManager fxManager;
    std::vector<Enemy> enemies;
    std::vector<DroppedItem> droppedItems;
    std::vector<GameLog> logs;           // 画面左下に表示されるシステムメッセージ
    std::vector<ItemData> storageItems;  // 倉庫(Storage)に預けた消費アイテム・素材
    std::vector<ItemData> storageEquip;  // 倉庫(Storage)に預けた装備品

    Camera3D camera;
    GameState state; // 現在のゲーム状態（タイトル、ホーム、ダンジョン等）

    int floor; // 現在の階層

    int currentDungeonId;  // 現在潜っているダンジョンのID (0:第1層, 1:第2層, 2:最深部)
    int unlockedDungeonId; // 解放済みのダンジョンの最大ID
    std::vector<int> maxFloors; // 各ダンジョンごとの最大到達階層（ワープ解放用）

    int currentSlot; // 現在プレイ中のセーブスロット番号(1〜3)
    int hoveredEntranceIndex; // ホーム拠点でプレイヤーが近づいているダンジョン入口のインデックス

    // --- UI表示状態を管理するフラグ群 ---
    bool debugMode;       // 開発者用デバッグモード(F1キー)のON/OFF
    bool showMenu;        // メインメニュー(TABキー)のON/OFF
    bool showStorage;     // 倉庫UIのON/OFF
    bool showReforgeMenu; // リフォージUIのON/OFF
    bool showWarpMenu;    // ワープUIのON/OFF
    bool showCraftMenu;   // クラフトUIのON/OFF
    bool showQuestMenu;   // クエストボードUIのON/OFF
    bool showPrompt;      // 「〇〇しますか？ はい/いいえ」のダイアログ表示ON/OFF
    MenuTab currentTab;   // メニュー画面で現在開いているタブ
    float sceneTimer;     // 画面遷移（フロア移動時など）の硬直・待機タイマー

    bool bossDefeated;    // 現在の階層のボスを倒したかどうか
    bool isPortfolioMode; // ポートフォリオ(ボスラッシュ)モード中かどうかのフラグ
};