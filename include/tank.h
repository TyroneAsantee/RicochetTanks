#ifndef TANK_H
#define TANK_H

#include <SDL.h>

void initTank(SDL_Renderer* renderer);
void drawTank(SDL_Renderer* renderer, SDL_Rect* dst);
void destroyTank();

#endif
