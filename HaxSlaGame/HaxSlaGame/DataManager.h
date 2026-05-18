#pragma once
#include "Definitions.h"
#include <vector>
#include <map>
#include <string>

class Player;

// 3Dモデルとそのテクスチャ、アニメーションデータをまとめて管理する構造体
struct GameModel {
    Model model;
    Texture2D texture;
    ModelAnimation* anims;
    int animCount;
    bool loaded;
};

class DataManager {
public:
    // JSONから読み込んだマスターデータのリスト
    static std::vector<EnemyData> allEnemyData;
    static std::vector<ItemData> itemConfigs;
    static std::map<std::string, std::string> uiStrings; // 多言語対応テキスト
    static std::vector<Modifier> modifiers;
    static std::vector<CraftRecipe> recipes;
    static std::vector<QuestData> quests;

    // メモリにロード済みの3Dモデルを名前(キー)で管理する辞書
    static std::map<std::string, GameModel> loadedModels;

    // コウモリ敵など、ハードコーディングされたモデルのフォールバック用
    static Model batModel;
    static Texture2D batTexture;
    static ModelAnimation* batAnims;
    static int batAnimCount;
    static bool isBatModelLoaded;

    // モデル読み込み失敗時に手や地面に描画される代替モデル(赤い直方体)
    static Model fallbackWeaponModel;
    // タイトル画面の背景画像
    static Texture2D titleBg;

    static KeyConfig keyConfig;
    static void LoadConfig();
    static void SaveConfig();
    static void ResetConfig(); // コンフィグをデフォルトにリセットする

    static void LoadAllData(); // 全JSONと必要な3Dモデルを一括ロード
    static void UnloadAllData(); // メモリの解放

    // 指定階層に出現可能な敵をランダムに取得する
    static EnemyData GetRandomEnemyForFloor(int floor, int dungeonId);
    static EnemyData GetBossEnemy(int floor, int dungeonId);

    static ItemData GetItemConfigCopy(int id);
    static QuestData GetQuestData(int id);
    static Modifier GetModifier(int id);
    static int GetRandomModifierId();

    // --- セーブ・ロード機能 ---
    static void SaveGame(int slot, Player* p, int currentFloor, int currentDungeon, int unlockedDungeon, const std::vector<int>& maxFloors, const std::vector<ItemData>& sItems, const std::vector<ItemData>& sEquip, bool isPortfolio);
    static bool LoadGame(int slot, Player* p, int& currentFloor, int& currentDungeon, int& unlockedDungeon, std::vector<int>& maxFloors, std::vector<ItemData>& sItems, std::vector<ItemData>& sEquip, bool& isPortfolio);
    static SaveHeader GetSaveHeader(int slot);

    static void DeleteSave(int slot);
};