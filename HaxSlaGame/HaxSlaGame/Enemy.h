#pragma once
#include "Definitions.h"
#include "raymath.h"

class Player;
class Dungeon;
class EffectManager;

class Enemy {
public:
    Vector3 position, lastAttackDir, patrolTarget;
    EnemyState state; // 現在の状態 (巡回、追跡、攻撃)
    EnemyType eType;  // 近接、遠距離、魔法などのタイプ
    EnemyData data;   // JSONから読み込んだマスターデータ
    float hp, maxHp, speed, radius, detectRange, attackRange, attackTimer, hudTimer;
    int level, expValue;

    Vector3 lastPos;
    int stuckCount; // 壁に引っかかって動けない状態を検知するカウンター

    // --- アニメーション管理 ---
    int animFrameCounter;
    bool isDying;
    bool isDead;

    // --- ボス専用AIフラグ ---
    bool isBoss;
    int bossAttackType;   // 現在実行中の大技の番号(1:3連撃, 2:弾幕, 3:高速突進, 4:巨大範囲攻撃)
    int bossComboStep;    // コンボ攻撃中のステップ数
    float bossActionTimer; // 予兆(Telegraph)を見せている間のタメ時間
    Vector3 bossTargetDir; // 攻撃を放つ方向

    Enemy(Vector3 sp, EnemyData d, int fl);

    void Update(Player& p, Dungeon& d, EffectManager& fx);
    void Draw(bool debug, Camera3D cam, Font font, Vector3 playerPos);
    void ApplyKnockback(Vector3 dir, float force, Dungeon& d);

    void StartDeath();

private:
    // 壁を避けて滑るように移動するスマートムーブ
    bool MoveSmart(Vector3 t, Dungeon& d);
};