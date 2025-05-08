#ifndef TANK_SERVER_H
#define TANK_SERVER_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <stdbool.h>

typedef struct {
    IPaddress address;
    int playerID;
    bool active;
} Player;

typedef struct {
    Uint32 lastHeartbeat;
    bool active;
} PlayerStatus;

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