#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "tank.h"
#include "timer.h"

int main(int argc, char** argv) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL Init Error: %s\n", SDL_GetError());
        return 1;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("SDL_image Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return 1;
    }

    // Skapa en osynlig renderer för att kunna ladda bilden (behövs av SDL)
    SDL_Window* dummyWindow = SDL_CreateWindow("Dummy", 0, 0, 1, 1, SDL_WINDOW_HIDDEN);
    SDL_Renderer* dummyRenderer = SDL_CreateRenderer(dummyWindow, -1, SDL_RENDERER_ACCELERATED);

    // Ladda bilden (men visa inte)
    initTank(dummyRenderer);

    // Destroy direkt efter laddning om inget mer ska göras nu
    destroyTank();
    SDL_DestroyRenderer(dummyRenderer);
    SDL_DestroyWindow(dummyWindow);

    IMG_Quit();
    SDL_Quit();
    return 0;
}
