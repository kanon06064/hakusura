#pragma once
#include "Definitions.h" 
#include <vector>
#include <string>

class Dungeon;
class Enemy;
class EffectManager;

class Player {
public:
    // --- ステータス・座標関連 ---
    Vector3 position, lastAimDir; // 座標と最後にエイム(向いた)した方向
    float speed, baseSpeed, radius, attackTimer, hp, maxHp, attackPower, defense;
    int level, exp, expToNext, skillPoints, activeSlot;
    int gold;
    bool isAttacking;

    // --- アニメーション管理変数 ---
    float animTime;           // アニメーションの再生時間(フレーム)
    int currentAnimIndex;     // 現在再生中のアニメーション番号(0:剣, 1:斧, 2:杖, 4:待機, 5:走り)
    int prevAnimIndex;
    float blendWeight;
    bool isTransitioning;
    float modelRotation;      // プレイヤーモデルの向いている角度
    bool isDead;              // 死亡フラグ

    // --- スキルとクールダウンの管理 ---
    float dashTimer, dashCooldownTimer;
    float smashCooldownTimer;
    float stealthTimer, stealthCooldownTimer;
    float kongoTimer, kongoCooldownTimer;
    float zoukyouTimer, zoukyouCooldownTimer;
    float healCooldownTimer;
    float cooldownReduction; // スキルによるクールダウン短縮率
    float healBonus;         // スキルによる回復量ボーナス
    bool isStealth;          // 隠密状態フラグ

    // --- 武器アタッチメント微調整用変数 (F1デバッグメニューで変更可能) ---
    static Vector3 customWeaponOffsetPos;
    static Vector3 customWeaponOffsetRot;
    static float customWeaponScale;

    // --- 装備・インベントリ・スキルツリー・クエスト ---
    WeaponType equippedWeapons[2], currentWeapon; // 武器スロット2つ
    ItemData equippedData[2];
    ItemData equippedArmor[5];                    // 防具5部位

    std::vector<ItemData> inventoryItems;         // 消耗品や素材
    std::vector<ItemData> inventoryEquip;         // 未装備の武器・防具
    std::vector<SkillNode> skillTree;             // スキルツリーのノード一覧

    std::vector<PlayerQuest> activeQuests;        // 受注中のクエスト
    std::vector<int> clearedQuests;               // 完了済みのクエストID

    Player(Vector3 sp); // コンストラクタ

    void Update(Camera3D& cam, Dungeon& d, std::vector<Enemy>& enemies, EffectManager& fx, bool stop);
    void Draw(bool debug); // プレイヤー本体と、手に持った武器の描画を行う

    void AddExp(int a, EffectManager& fx); // 経験値獲得・レベルアップ処理
    bool AddToInventory(ItemData item);    // アイテムを拾う
    void UseItem(int idx);                 // アイテムを使う

    void EquipWeapon(int invIdx, int slot);
    void UnequipWeapon(int slot);
    void EquipArmor(int invIdx, int slot);
    void UnequipArmor(int slot);

    void UnlockSkill(int id);
    bool IsSkillAvailable(int id);
    bool IsSkillUnlocked(SkillType type);
    float GetSkillCooldown(SkillType type);
    float GetSkillMaxCooldown(SkillType type);

    void RecalculateStats(); // 装備やスキルによるステータス再計算

    void UpdateHuntQuest(int enemyId); // 敵を倒したときに討伐クエストを進める
    bool CheckGatherQuest(int itemId, int requiredCount); // 収集クエストの条件を満たしているか
    void CompleteQuest(int questId); // クエスト報告・報酬受け取り

    static std::string GetFullItemName(const ItemData& item);
    static float GetItemTotalAtkBonus(const ItemData& item);
    static Color GetItemRarityColor(const ItemData& item);

private:
    void PerformAttack(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void PerformSmash(Vector3 ad, std::vector<Enemy>& enemies, Dungeon& d, EffectManager& fx);
    void LevelUp(EffectManager& fx);
    void InitSkillTree();
};