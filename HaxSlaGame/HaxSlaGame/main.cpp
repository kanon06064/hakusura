#include "Game.h"
#include <time.h>
#include <stdlib.h>

int main() {
    // 乱数初期化
    srand((unsigned int)time(NULL));

    // GameクラスがWindow生成からループ管理まで行う
    Game game;
    game.Run();

    return 0;
}