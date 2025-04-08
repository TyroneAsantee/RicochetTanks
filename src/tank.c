#include <SDL.h>
#include <SDL_image.h>
#include "tank.h"
#include "timer.h"

static SDL_Texture* tankTexture = NULL;

void initTank(SDL_Renderer* renderer) {
    tankTexture = IMG_LoadTexture(renderer, "resources/tank.png");
    if (!tankTexture) {
        SDL_Log("Kunde inte ladda tank.png: %s", IMG_GetError());
    }
}

void drawTank(SDL_Renderer* renderer) {
    if (!tankTexture) return;

    SDL_Rect dst = { 300, 300, 100, 100 }; // position + storlek
    SDL_RenderCopy(renderer, tankTexture, NULL, &dst);
}

void destroyTank() {
    if (tankTexture) {
        SDL_DestroyTexture(tankTexture);
        tankTexture = NULL;
    }
}