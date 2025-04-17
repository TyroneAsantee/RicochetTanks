#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static SDL_Texture* tankTexture = NULL;
static SDL_Texture* heartTexture = NULL;


void initTank(SDL_Renderer* renderer) {
    tankTexture = IMG_LoadTexture(renderer, "resources/tank.png");
    if (!tankTexture) {
        SDL_Log("Kunde inte ladda tank.png: %s", IMG_GetError());
    }
}

void drawTank(SDL_Renderer* renderer, Tank* tank) {
    if (!tankTexture) return;
    SDL_RenderCopy(renderer, tankTexture, NULL, &tank->rect);
}

void loadHeartTexture(SDL_Renderer* renderer) {
    SDL_Surface* surface = IMG_Load("resources/heart.png");
    if (!surface) {
        SDL_Log("Kunde inte ladda heart.png: %s", IMG_GetError());
        return;
    }
    heartTexture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if (!heartTexture) {
        SDL_Log("Kunde inte skapa heart-texture: %s", SDL_GetError());
    }
}

void destroyHeartTexture(void) {
    if (heartTexture) {
        SDL_DestroyTexture(heartTexture);
        heartTexture = NULL;
    }
}

void renderTankHealth(SDL_Renderer* renderer, int health) {
    if (!heartTexture) return;

    SDL_Rect heartRect = {0, 10, 32, 32};  // uppe på skärmen
    for (int i = 0; i < health; i++) {
        heartRect.x = 800 - (i + 1) * 40;  // 760, 720, 680
        SDL_RenderCopy(renderer, heartTexture, NULL, &heartRect);
    }
}


void tankmovement(SDL_Texture* pTankpicture, SDL_Texture* pBackground, Tank* tank, SDL_Renderer* pRenderer, SDL_Event event)
{
    float tankX = (WINDOW_WIDTH - tank->rect.w) / 2;
    float tankY = (WINDOW_HEIGHT - tank->rect.h) / 2;
    float tankVelocityX = 0;
    float tankVelocityY = 0;
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
                        case SDL_SCANCODE_UP:
                            up = true;
                            break;
                        case SDL_SCANCODE_S:
                        case SDL_SCANCODE_DOWN:
                            down = true;
                            break;
                        case SDL_SCANCODE_A:
                        case SDL_SCANCODE_LEFT:
                            angle -= 10.0f;
                            break;
                        case SDL_SCANCODE_D:
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
                        case SDL_SCANCODE_UP:
                            up = false;
                            break;
                        case SDL_SCANCODE_S:
                        case SDL_SCANCODE_DOWN:
                            down = false;
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }

        tankVelocityX = 0;
        tankVelocityY = 0;

        if (up && !down) {
            float radians = (angle - 90.0f) * M_PI / 180.0f;
            tankVelocityX = cos(radians) * SPEED;
            tankVelocityY = sin(radians) * SPEED;
        }
        if (down && !up) {
            float radians = (angle - 90.0f) * M_PI / 180.0f;
            tankVelocityX = -cos(radians) * SPEED;
            tankVelocityY = -sin(radians) * SPEED;
        }

        tankX += tankVelocityX / 60;
        tankY += tankVelocityY / 60;

        if (tankX < 0) tankX = 0;
        if (tankY < 0) tankY = 0;
        if (tankX > WINDOW_WIDTH - tank->rect.w) tankX = WINDOW_WIDTH - tank->rect.w;
        if (tankY > WINDOW_HEIGHT - tank->rect.h) tankY = WINDOW_HEIGHT - tank->rect.h;

        tank->rect.x = tankX;
        tank->rect.y = tankY;
        tank->angle = angle;

        SDL_RenderClear(pRenderer);
        SDL_RenderCopy(pRenderer, pBackground, NULL, NULL);
        SDL_RenderCopyEx(pRenderer, pTankpicture, NULL, &tank->rect, angle, NULL, SDL_FLIP_NONE);
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
