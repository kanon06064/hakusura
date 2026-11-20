#include "Game.h"
#include <time.h>
#include <stdlib.h>

int main() {
    // 乱数シードの初期化
    srand((unsigned int)time(NULL));

    // Gameクラスのインスタンス生成
    Game game;

    // ゲームのメインループ開始
    game.Run();

    return 0;
}