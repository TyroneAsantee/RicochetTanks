#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "tank.h"
#include "timer.h"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define SPEED 100
struct game
{
    SDL_Window *pWindow;
    SDL_Renderer *pRenderer;
    SDL_Texture *pBackground;
    SDL_Texture *pTankpicture;
    SDL_Rect shipRect;
    SDL_Event event;
    Timer timer;
    float angle;
};
typedef struct game Game;

void initiate(Game* game);                  // Initiera SDL miljö och fönster
void run(Game* game);                       // Hanterar själva spelet
void close(Game* game);                     // Hanterar stängning och uppstädning av SDL miljön


int main(void){
    Game game;
    initiate(&game);
    run(&game);
    close(&game);
}

void initiate(Game *game){

     if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL Init Error: %s\n", SDL_GetError());
        return;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("SDL_image Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return;
    }

    game->pWindow = SDL_CreateWindow("Ricochet Tanks", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!game->pWindow) 
    {
        printf("Window error: %s\n", SDL_GetError());
        return;
    }
    
    game->pRenderer = SDL_CreateRenderer(game->pWindow, -1, SDL_RENDERER_ACCELERATED);
    if (!game->pRenderer) 
    {
        printf("Renderer error: %s\n", SDL_GetError());
        SDL_DestroyWindow(game->pWindow);
        SDL_Quit();
        return;
    }
    SDL_RaiseWindow(game->pWindow);
    
    // Ladda bilden (men visa inte)
    initTank(game->pRenderer);
    game->shipRect.w = 64;
    game->shipRect.h = 64;
    game->shipRect.x = (WINDOW_WIDTH - game->shipRect.w) / 2;
    game->shipRect.y = (WINDOW_HEIGHT - game->shipRect.h) / 2 - 10;


    SDL_Surface *pBgSurface = IMG_Load("resources/background.png");
    if(!pBgSurface)
    {
        printf("Could not load background: %s\n", IMG_GetError());
        return;
    }
    game->pBackground = SDL_CreateTextureFromSurface(game->pRenderer, pBgSurface);
    SDL_FreeSurface(pBgSurface);

    if(!game->pBackground)
    {
        printf("Could not create texture for background: %s\n", SDL_GetError());
        return;
    }

    SDL_RenderClear(game->pRenderer);
    SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);
    drawTank(game->pRenderer, &game->shipRect);  //från tank.c
    SDL_RenderPresent(game->pRenderer);    
}

void run(Game *game){                                //TESTKOD FÖR ATT SE OM ALLT FUNKAR
    SDL_Surface *pTank = IMG_Load("resources/tank.png");
    if(!pTank)
    {
        printf("Could not load background: %s\n", IMG_GetError());
        return;
    }
    game->pTankpicture = SDL_CreateTextureFromSurface(game->pRenderer, pTank);
    SDL_FreeSurface(pTank);

    if(!game->pBackground)
    {
        printf("Could not create texture for background: %s\n", SDL_GetError());
        return;
    }
    SDL_QueryTexture(game->pTankpicture, NULL, NULL, &game->shipRect.w, &game->shipRect.h);
    game->shipRect.w /= 64;
    game->shipRect.h /= 64;
    tankmovement(game->pTankpicture, game->pBackground, &game->shipRect, game->pRenderer, game->event);
}



void close(Game *game)
{
    destroyTank();
    SDL_DestroyRenderer(game->pRenderer);
    SDL_DestroyWindow(game->pWindow);
    IMG_Quit();
    SDL_Quit();
}