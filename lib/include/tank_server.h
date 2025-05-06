#ifndef TANK_SERVER_H
#define TANK_SERVER_H

#include <SDL2/SDL.h>

typedef struct Tank Tank;

Tank* createTank();
void destroyTankInstance(Tank* tank);

void setTankPosition(Tank* tank, int x, int y);
void setTankAngle(Tank* tank, float angle);
void setTankHealth(Tank* tank, int health);
void setTankColorId(Tank* tank, int id);

int getTankHealth(const Tank* tank);
float getTankAngle(const Tank* tank);
int getTankColorId(const Tank* tank);
SDL_Rect getTankRect(const Tank* tank);

#endif