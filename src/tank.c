#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#define SPEED 100
#define WINDOW_WIDTH 600
#define WINDOW_HEIGHT 400

static SDL_Texture* tankTexture = NULL;

void initTank(SDL_Renderer* renderer) {
    tankTexture = IMG_LoadTexture(renderer, "resources/tank.png");
    if (!tankTexture) {
        SDL_Log("Kunde inte ladda tank.png: %s", IMG_GetError());
    }
}

void drawTank(SDL_Renderer* renderer, SDL_Rect* dst) {
    if (!tankTexture) return;

    SDL_RenderCopy(renderer, tankTexture, NULL, dst);
}

void destroyTank() {
    if (tankTexture) {
        SDL_DestroyTexture(tankTexture);
        tankTexture = NULL;
    }
}