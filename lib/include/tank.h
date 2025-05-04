#ifndef TANK_H
#define TANK_H

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdlib.h>

#define SPEED 100

typedef struct Tank Tank;

Tank* createTank(void);
void destroyTankInstance(Tank* tank);

void setTankPosition(Tank* tank, int x, int y);
void setTankAngle(Tank* tank, float angle);
void setTankHealth(Tank* tank, int health);
bool isTankAlive(Tank* tank);
int getTankHealth(const Tank* tank);

SDL_Rect getTankRect(Tank* tank);
float getTankAngle(Tank* tank);

void initTank(SDL_Renderer* renderer);
void drawTank(SDL_Renderer* renderer, Tank* tank, SDL_Texture* texture);
void loadHeartTexture(SDL_Renderer* renderer);
void destroyHeartTexture(void);
void renderTankHealth(SDL_Renderer* renderer, int health);
void destroyTank(void);

#endif
