#include "Game.h"
#include <time.h>
#include <stdlib.h>

int main() {
    // 乱数のシード(種)を現在の時間で初期化し、実行ごとにランダムな結果が得られるようにする
    srand((unsigned int)time(NULL));

    // Gameクラスのインスタンス生成
    Game game;

    // ゲームのメインループを開始 (ウィンドウを閉じるまでここから抜けない)
    game.Run();

    return 0;
}