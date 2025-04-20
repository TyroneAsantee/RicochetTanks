#ifndef TANK_H
#define TANK_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdbool.h>
#include <math.h>
#define SPEED 100

typedef struct
{
    SDL_Rect rect;
    float velocityX;
    float velocityY;
    float angle;
    int health;
    bool alive;
} Tank;

void initTank(SDL_Renderer* renderer);
SDL_Texture* getTankTexture(void);
void tankmovement(SDL_Texture* pTankpicture, SDL_Texture* pBackground, Tank* tank, SDL_Renderer* pRenderer, SDL_Event event);
void drawTank(SDL_Renderer* renderer, Tank* tank);
void loadHeartTexture(SDL_Renderer* renderer);
void destroyHeartTexture(void);
void renderTankHealth(SDL_Renderer* renderer, int health);
void destroyTank(void);

#endif
