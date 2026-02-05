#include "Game.h"
#include <time.h>
#include <stdlib.h>

int main() {
    srand((unsigned int)time(NULL));

    Game game;
    game.Run();

    return 0;
}