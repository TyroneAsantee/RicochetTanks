#ifndef TANK_H
#define TANK_H

#include <SDL.h>
#include <stdbool.h>
#include <math.h>

void initTank(SDL_Renderer* renderer);
SDL_Texture* getTankTexture(void);
void drawTank(SDL_Renderer* renderer, SDL_Rect* dst);
void destroyTank();

#endif
