#ifndef TANK_H
#define TANK_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <math.h>

void initTank(SDL_Renderer* renderer);
SDL_Texture* getTankTexture(void);
void tankmovement(SDL_Texture *pTankpicture, SDL_Texture *pBackground, SDL_Rect *shipRect, SDL_Renderer *pRenderer, SDL_Event event);
void drawTank(SDL_Renderer* renderer, SDL_Rect* dst);
void destroyTank();

#endif
