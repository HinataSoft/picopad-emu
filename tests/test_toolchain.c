#include <SDL.h>
#include "check.h"

int main(int argc, char **argv)
{
    (void)argc; (void)argv;
    CHECK_EQ(SDL_Init(0), 0);
    CHECK_EQ((long long)sizeof(void *), 4);
    SDL_Quit();
    return CHECK_DONE();
}
