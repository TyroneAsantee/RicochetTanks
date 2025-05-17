#include "tank_server.h"
#include <stdlib.h>
#include <SDL.h>

struct Tank {
    int x, y;
    float angle;
    int health;
    int colorId;
};

Tank* createTank() {
    Tank* tank = malloc(sizeof(Tank));
    tank->x = 0;
    tank->y = 0;
    tank->angle = 0.0f;
    tank->health = 3;
    tank->colorId = 0;
    return tank;
}

void setTankPosition(Tank* tank, int x, int y) {
    tank->x = x;
    tank->y = y;
}

void setTankAngle(Tank* tank, float angle) {
    tank->angle = angle;
}

void setTankHealth(Tank* tank, int health) {
    tank->health = health;
}

void setTankColorId(Tank* tank, int id) {
    tank->colorId = id;
}

int getTankHealth(const Tank* tank) {
    return tank->health;
}

float getTankAngle(const Tank* tank) {
    return tank->angle;
}

int getTankColorId(const Tank* tank) {
    return tank->colorId;
}

SDL_Rect getTankRect(const Tank* tank) {
    SDL_Rect rect = { tank->x, tank->y, 64, 64 };
    return rect;
}

void destroyTankInstance(Tank* tank) {
    free(tank);
}