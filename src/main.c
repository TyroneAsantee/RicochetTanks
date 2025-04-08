#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include "tank.h"
#include "timer.h"
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

struct game
{
    SDL_Window *pWindow;
    SDL_Renderer *pRenderer;
    SDL_Texture *pBackground;
    SDL_Rect tank;
    Timer timer;
};
typedef struct game Game;

void initiate(Game* game);                  // Initiera SDL miljö och fönster
void run(Game* game);                       // Hanterar själva spelet
void close(Game* game);                     // Hanterar stängning och uppstädning av SDL miljön


int main(int argv, char** args)
{
    Game game;

    initiate(&game);
    run(&game);
    close(&game);
    
}

void initiate(Game *game)
{
     if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        printf("SDL Init Error: %s\n", SDL_GetError());
        return;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        printf("SDL_image Init Error: %s\n", IMG_GetError());
        SDL_Quit();
        return;
    }

    game->pWindow = SDL_CreateWindow("Tankspel", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                     WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
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
    game->tank.x = 300;
    game->tank.y = 300;
    game->tank.w = 100;
    game->tank.h = 100;

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
    drawTank(game->pRenderer);                          //från tank.c
    SDL_RenderPresent(game->pRenderer);    
}

void run(Game *game)                                    //TESTKOD FÖR ATT SE OM ALLT FUNKAR
{
    int running = 1;
    SDL_Event event;

    while (running) 
    {
        while (SDL_PollEvent(&event)) 
        {
            if (event.type == SDL_QUIT || 
               (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = 0;
            }
        }

        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);
        drawTank(game->pRenderer);  // Från tank.c
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16); // ~60 FPS
    }
}



void close(Game *game)
{
    destroyTank();
    SDL_DestroyRenderer(game->pRenderer);
    SDL_DestroyWindow(game->pWindow);
    IMG_Quit();
    SDL_Quit();
}