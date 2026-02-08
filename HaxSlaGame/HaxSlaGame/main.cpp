#include "Game.h"
#include <time.h>
#include <stdlib.h>

int main() {
    // 乱数の初期化
    srand((unsigned int)time(NULL));

    // Gameクラスのインスタンス生成と実行
    // ウィンドウの初期化やメインループはGameクラス内部で行われます
    Game game;
    game.Run();

    return 0;
}