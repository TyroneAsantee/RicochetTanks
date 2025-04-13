#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#define SPEED 100
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

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
    game->shipRect.w = 100;
    game->shipRect.h = 100;
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
    game->shipRect.w /= 60;
    game->shipRect.h /= 64;

    float shipX = (WINDOW_WIDTH - game->shipRect.w) / 2;
    float shipY = (WINDOW_HEIGHT - game->shipRect.h) / 2;
    float shipVelocityX = 0;
    float shipVelocityY = 0;
    float angle = 0.0f;

    bool closeWindow = false;
    bool up = false, down = false;

    while (!closeWindow) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    closeWindow = true;
                    break;
                case SDL_KEYDOWN:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_W:
                            up = true;
                            break;
                        case SDL_SCANCODE_S:
                            down = true;
                            break;
                        case SDL_SCANCODE_A:
                            angle -= 10.0f;
                            break;
                        case SDL_SCANCODE_D:
                            angle += 10.0f;
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (event.key.keysym.scancode) {
                        case SDL_SCANCODE_W:
                            up = false;
                            break;
                        case SDL_SCANCODE_S:
                            down = false;
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }

        shipVelocityX = 0;
        shipVelocityY = 0;

        // Räkna ut rörelse baserat på riktning och vinkel — justerat med -90° för att peka uppåt
        if (up && !down) {
            float radians = (angle - 90.0f) * M_PI / 180.0f;
            shipVelocityX = cos(radians) * SPEED;
            shipVelocityY = sin(radians) * SPEED;
        }
        if (down && !up) {
            float radians = (angle - 90.0f) * M_PI / 180.0f;
            shipVelocityX = -cos(radians) * SPEED;
            shipVelocityY = -sin(radians) * SPEED;
        }

        shipX += shipVelocityX / 60;
        shipY += shipVelocityY / 60;

        // Håll inom fönstret
        if (shipX < 0) shipX = 0;
        if (shipY < 0) shipY = 0;
        if (shipX > WINDOW_WIDTH - game->shipRect.w) shipX = WINDOW_WIDTH - game->shipRect.w;
        if (shipY > WINDOW_HEIGHT - game->shipRect.h) shipY = WINDOW_HEIGHT - game->shipRect.h;

        game->shipRect.x = shipX;
        game->shipRect.y = shipY;

        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);
        SDL_RenderCopyEx(game->pRenderer, game->pTankpicture, NULL, &game->shipRect, angle, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(1000 / 60);
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