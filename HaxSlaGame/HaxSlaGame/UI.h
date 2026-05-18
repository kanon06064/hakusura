#pragma once
#include "Definitions.h" 
#include "raylib.h"
#include <vector>
#include <string>

// 画面左下に表示されるシステムログ（「〇〇を入手した」など）の構造体
struct SystemLogMessage {
    std::string text;
    Color color;
    float lifeTime;
    float maxLifeTime;
};

class Player;
class Enemy;
class Dungeon;
class Game;

class UI {
public:
    // --- 各種画面の描画関数 ---
    // インゲームのHUD(HPバー、ミニマップ、スキルアイコンなど)を描画
    static void DrawHUD(class Player& p, std::vector<class Enemy>& enemies, class Dungeon& d, Camera3D& cam, int floor, int dungeonId, bool debug, Font font);
    // TABキーで開くメインメニュー(装備、スキル、インベントリなど)を描画
    static int DrawMenu(class Player& p, class Dungeon& d, MenuTab& tab, Font font);

    // ホーム拠点の各施設UIを描画
    static void DrawStorage(class Player& p, Font font, bool& isOpen, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip);
    static void DrawReforgeMenu(class Player& p, Font font, bool& isOpen);
    static void DrawCraftingMenu(class Player& p, Font font, bool& isOpen);
    static void DrawQuestMenu(class Player& p, Font font, bool& isOpen);

    // タイトル画面の描画（戻り値で選択されたセーブスロットを返す）
    static int DrawTitleScreen(Font font);
    // ポータルによるダンジョンワープ画面の描画
    static void DrawWarpMenu(void* gamePtr, int unlockedDungeon, const std::vector<int>& maxFloors, Font font, bool& isOpen);

    // 「〇〇しますか？ はい/いいえ」のダイアログを描画
    static int DrawPrompt(const char* label, int sw, int sh, Font font);

    // プレイヤーの頭上に浮かぶログと、落ちているアイテム名の描画（3D空間連動）
    static void DrawLogs(std::vector<GameLog>& logs, class Player& p, Camera3D& cam, Font font);
    static void DrawNearbyItems(class Player& p, std::vector<DroppedItem>& di, class Dungeon& d, Camera3D& cam, Font font);

    // 共通のボタン描画関数（押されたら true を返す）
    static bool DrawButton(Rectangle rect, const char* label, Font font, Color baseCol);

    // --- システムログ関連 ---
    static void AddSystemLog(const std::string& text, Color color = WHITE);
    static void UpdateSystemLogs(float deltaTime);
    static void DrawSystemLogs(Font font);

    // ゲームパッドのボタン定数を文字列("A"や"R1"など)に変換する
    static std::string GetPadBtnStr(int btn);

    // --- ゲームパッドのUIナビゲーション（スナップ移動）機構 ---
    static void ClearInteractables();             // 毎フレーム、登録されたボタン情報をリセット
    static void RegisterInteractable(Rectangle r); // ボタンを描画した際にその座標を登録
    static void UpdatePadNavigation();            // 十字キーの入力に応じてマウスカーソルをボタンに吸い付かせる

    // --- UIのページ遷移・スクロール状態を保持する変数 ---
    static int itemPage, equipPage, storageInvPage, storageBoxPage, itemSubTab;
    static int selectedDungeonTab;
    static int questScroll;

    // --- アイテム詳細画面(サブウィンドウ)の管理 ---
    static bool showDetail;
    static ItemData focusingItem;
    static float detailOpenTimer; // 開いた直後に誤爆クリックしないためのタイマー
    static int deleteConfirmSlot; // セーブデータ削除の確認状態を保持

private:
    static int reforgeItemIdx;
    static int warpScroll;
    static int craftingScroll;
    static Vector2 skillOffset; // スキルツリーのドラッグ移動オフセット
    static Vector2 mapOffset;   // マップタブのドラッグ移動オフセット

    static std::vector<SystemLogMessage> systemLogs;
    static std::vector<Rectangle> interactables; // 画面上のクリック可能なボタン領域のリスト

    static void DrawDetailWindow(Font font);
    static void OpenDetail(const ItemData& item);
};