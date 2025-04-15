#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include "tank.h"
#include "timer.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

typedef struct {
    SDL_Window *pWindow;
    SDL_Renderer *pRenderer;
    SDL_Texture *pBackground;
    SDL_Texture *pTankpicture;
    SDL_Event event;
    float angle;
    Timer timer;
    Tank tank;                      //Använder Tank-structen
} Game;

void initiate(Game* game);
void run(Game* game);
void close(Game* game);

int main(void) {
    Game game;
    initiate(&game);
    run(&game);
    close(&game);
    return 0;
}

void initiate(Game *game) 
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL Init Error: %s", SDL_GetError());
        return;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("SDL_image Init Error: %s", IMG_GetError());
        SDL_Quit();
        return;
    }

    game->pWindow = SDL_CreateWindow("Ricochet Tank", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!game->pWindow) {
        SDL_Log("Window error: %s", SDL_GetError());
        return;
    }

    game->pRenderer = SDL_CreateRenderer(game->pWindow, -1, SDL_RENDERER_ACCELERATED);
    if (!game->pRenderer) {
        SDL_Log("Renderer error: %s", SDL_GetError());
        SDL_DestroyWindow(game->pWindow);
        SDL_Quit();
        return;
    }

    initTank(game->pRenderer);

    // Initiera tankens värden
    game->tank.rect.x = (WINDOW_WIDTH - 64) / 2;
    game->tank.rect.y = (WINDOW_HEIGHT - 64) / 2;
    game->tank.rect.w = 64;
    game->tank.rect.h = 64;
    game->tank.angle = 0.0f;
    game->tank.velocityX = 0;
    game->tank.velocityY = 0;
    game->tank.health = 100;

    SDL_Surface *pTankSurface = IMG_Load("resources/tank.png");
    game->pTankpicture = SDL_CreateTextureFromSurface(game->pRenderer, pTankSurface);
    SDL_FreeSurface(pTankSurface);

    SDL_Surface *bgSurface = IMG_Load("resources/background.png");
    if (!bgSurface) {
        SDL_Log("Could not load background: %s", IMG_GetError());
        return;
    }
    game->pBackground = SDL_CreateTextureFromSurface(game->pRenderer, bgSurface);
    SDL_FreeSurface(bgSurface);
}

void run(Game *game) 
{
    tankmovement(game->pTankpicture, game->pBackground, &game->tank, game->pRenderer, game->event);
}

void close(Game *game) 
{
    destroyTank();
    SDL_DestroyTexture(game->pTankpicture);
    SDL_DestroyTexture(game->pBackground);
    SDL_DestroyRenderer(game->pRenderer);
    SDL_DestroyWindow(game->pWindow);
    IMG_Quit();
    SDL_Quit();
}
