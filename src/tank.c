#include <SDL.h>
#include <SDL_image.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#define SPEED 100
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

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
void tankmovement(SDL_Texture *pTankpicture, SDL_Texture *pBackground, SDL_Rect *shipRect, SDL_Renderer *pRenderer, SDL_Event event)
{
    float shipX = (WINDOW_WIDTH - shipRect->w) / 2;
    float shipY = (WINDOW_HEIGHT - shipRect->h) / 2;
    float shipVelocityX = 0;
    float shipVelocityY = 0;
    float angle = 0.0f;

    bool closeWindow = false;
    bool up = false, down = false;

    while (!closeWindow) {
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
                        case SDL_SCANCODE_UP:
                            up = true;
                            break;    
                        case SDL_SCANCODE_S:
                            down = true;
                            break;
                        case SDL_SCANCODE_DOWN:
                            down = true;
                            break;    
                        case SDL_SCANCODE_A:
                            angle -= 10.0f;
                            break;
                        case SDL_SCANCODE_LEFT:
                        angle -= 10.0f;
                        break;
                        case SDL_SCANCODE_D:
                            angle += 10.0f;
                            break;
                        case SDL_SCANCODE_RIGHT:
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
                        case SDL_SCANCODE_UP:
                        up = false;
                        break;
                        case SDL_SCANCODE_S:
                            down = false;
                            break;
                        case SDL_SCANCODE_DOWN:
                        down = false;
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
        if (shipX > WINDOW_WIDTH - shipRect->w) shipX = WINDOW_WIDTH - shipRect->w;
        if (shipY > WINDOW_HEIGHT - shipRect->h) shipY = WINDOW_HEIGHT - shipRect->h;

        shipRect->x = shipX;
        shipRect->y = shipY;

        SDL_RenderClear(pRenderer);
        SDL_RenderCopy(pRenderer, pBackground, NULL, NULL);
        SDL_RenderCopyEx(pRenderer, pTankpicture, NULL, shipRect, angle, NULL, SDL_FLIP_NONE);
        SDL_RenderPresent(pRenderer);
        SDL_Delay(1000 / 60);
    }
}

void destroyTank() {
    if (tankTexture) {
        SDL_DestroyTexture(tankTexture);
        tankTexture = NULL;
    }
}