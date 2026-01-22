#include "Player.h"
#include "Dungeon.h"
#include "Enemy.h"
#include "raymath.h"

Player::Player(Vector3 startPos) : position(startPos), speed(0.15f), radius(0.45f), currentWeapon(SWORD), attackTimer(0), visualTimer(0), isAttacking(false), lastAimDir({ 1,0,0 }), hp(100), maxHp(100) {}

void Player::Update(Camera3D& camera, Dungeon& dungeon, std::vector<Enemy>& enemies, std::vector<DamageText>& dmgTexts) {
    Vector3 camF = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    camF.y = 0; camF = Vector3Normalize(camF);
    Vector3 camR = { -camF.z, 0, camF.x };

    Vector3 moveDir = { 0 };
    if (IsKeyDown(KEY_W)) moveDir = Vector3Add(moveDir, camF);
    if (IsKeyDown(KEY_S)) moveDir = Vector3Subtract(moveDir, camF);
    if (IsKeyDown(KEY_A)) moveDir = Vector3Subtract(moveDir, camR);
    if (IsKeyDown(KEY_D)) moveDir = Vector3Add(moveDir, camR);

    if (Vector3Length(moveDir) > 0) {
        moveDir = Vector3Normalize(moveDir);
        Vector3 v = Vector3Scale(moveDir, speed);
        if (!dungeon.CheckCollisionRadius({ position.x + v.x, 0.5f, position.z }, radius)) position.x += v.x;
        if (!dungeon.CheckCollisionRadius({ position.x, 0.5f, position.z + v.z }, radius)) position.z += v.z;
    }

    if (IsKeyPressed(KEY_ONE)) currentWeapon = SWORD;
    if (IsKeyPressed(KEY_TWO)) currentWeapon = SPEAR;
    if (IsKeyPressed(KEY_THREE)) currentWeapon = AXE;
    if (IsKeyPressed(KEY_FOUR)) currentWeapon = BOW;
    if (IsKeyPressed(KEY_FIVE)) currentWeapon = WAND;

    if (attackTimer > 0) attackTimer -= GetFrameTime();
    if (visualTimer > 0) visualTimer -= GetFrameTime(); else isAttacking = false;

    if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && attackTimer <= 0) {
        Ray ray = GetMouseRay(GetMousePosition(), camera);
        float t = -ray.position.y / ray.direction.y;
        Vector3 g = Vector3Add(ray.position, Vector3Scale(ray.direction, t));
        lastAimDir = Vector3Normalize(Vector3Subtract(g, position)); lastAimDir.y = 0;
        PerformAttack(lastAimDir, enemies, dungeon, dmgTexts);
        attackTimer = (currentWeapon == BOW || currentWeapon == WAND) ? 0.3f : 0.5f;
        isAttacking = true; visualTimer = 0.15f;
    }

    for (int i = (int)projectiles.size() - 1; i >= 0; i--) {
        projectiles[i].pos = Vector3Add(projectiles[i].pos, Vector3Scale(projectiles[i].vel, GetFrameTime()));
        if (dungeon.IsWall(projectiles[i].pos.x, projectiles[i].pos.z)) { projectiles.erase(projectiles.begin() + i); continue; }
        for (auto& e : enemies) {
            if (Vector3Distance(projectiles[i].pos, e.position) < (projectiles[i].radius + e.radius)) {
                int dmg = GetRandomValue(10, 15); e.hp -= dmg; e.hudTimer = 5.0f;
                e.ApplyKnockback(Vector3Normalize(projectiles[i].vel), 0.5f, dungeon);
                dmgTexts.push_back({ {e.position.x, 1.5f, e.position.z}, dmg, 0.8f });
                projectiles.erase(projectiles.begin() + i); break;
            }
        }
    }
}

void Player::PerformAttack(Vector3 aimDir, std::vector<Enemy>& enemies, Dungeon& dungeon, std::vector<DamageText>& dmgTexts) {
    if (currentWeapon == BOW) projectiles.push_back({ position, Vector3Scale(aimDir, 25.0f), 0.2f, true, 0 });
    else if (currentWeapon == WAND) projectiles.push_back({ position, Vector3Scale(aimDir, 15.0f), 0.5f, true, 1 });
    else {
        int hitCount = 0;
        for (auto& e : enemies) {
            if (!dungeon.HasLineOfSight(position, e.position)) continue;
            Vector3 v = Vector3Subtract(e.position, position); float d = Vector3Length(v);
            bool hit = false; float knk = 0;
            switch (currentWeapon) {
            case SWORD: if (d < 4.5f && Vector3DotProduct(aimDir, Vector3Normalize(v)) > cosf(60 * DEG2RAD)) { hit = true; knk = 0.8f; } break;
            case SPEAR: { float f = Vector3DotProduct(v, aimDir), s = fabsf(Vector3DotProduct(v, { -aimDir.z,0,aimDir.x })); if (f > 0 && f < 8.2f && s < 1.3f) { hit = true; knk = 2.2f; } } break;
            case AXE: if (Vector3Distance(e.position, Vector3Add(position, Vector3Scale(aimDir, 3.5f))) < 3.5f) { hit = true; knk = 1.3f; } break;
            }
            if (hit) {
                int dmg = GetRandomValue(8, 15); e.hp -= dmg; e.hudTimer = 5.0f; e.ApplyKnockback(aimDir, knk, dungeon);
                dmgTexts.push_back({ {e.position.x, 1.5f - (hitCount * 0.4f), e.position.z}, dmg, 0.8f });
                hitCount++;
            }
        }
    }
}

void Player::Draw() {
    DrawCube(position, 1.0f, 1.0f, 1.0f, RED); DrawCubeWires(position, 1.0f, 1.0f, 1.0f, MAROON);
    for (auto& p : projectiles) { if (p.type == 0) DrawCube(p.pos, 0.2f, 0.2f, 0.6f, BROWN); else DrawSphere(p.pos, p.radius, SKYBLUE); }
    if (isAttacking) {
        float y = 0.1f; Color col = YELLOW;
        if (currentWeapon == SWORD) {
            float base = atan2f(lastAimDir.z, lastAimDir.x);
            for (int i = -60; i <= 60; i += 10) DrawLine3D({ position.x, y, position.z }, { position.x + cosf(base + i * DEG2RAD) * 4.5f, y, position.z + sinf(base + i * DEG2RAD) * 4.5f }, col);
        }
        else if (currentWeapon == SPEAR) {
            Vector3 r = { -lastAimDir.z,0,lastAimDir.x }, p1 = Vector3Add(position, Vector3Scale(r, 1.3f)), p2 = Vector3Subtract(position, Vector3Scale(r, 1.3f)), p3 = Vector3Add(p2, Vector3Scale(lastAimDir, 8.2f)), p4 = Vector3Add(p1, Vector3Scale(lastAimDir, 8.2f));
            p1.y = p2.y = p3.y = p4.y = y; DrawLine3D(p1, p2, col); DrawLine3D(p2, p3, col); DrawLine3D(p3, p4, col); DrawLine3D(p4, p1, col);
        }
        else if (currentWeapon == AXE) DrawSphereWires(Vector3Add(position, Vector3Scale(lastAimDir, 3.5f)), 3.5f, 8, 8, col);
    }
}