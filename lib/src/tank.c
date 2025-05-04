#include "tank.h"
#include "timer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static SDL_Texture* tankTexture = NULL;
static SDL_Texture* heartTexture = NULL;

struct Tank {
    SDL_Rect rect;
    float velocityX;
    float velocityY;
    float angle;
    int health;
    bool alive;
};

Tank* createTank(void) {
    Tank* tank = malloc(sizeof(Tank));
    if (tank) {
        tank->rect = (SDL_Rect){0, 0, 64, 64};
        tank->velocityX = 0;
        tank->velocityY = 0;
        tank->angle = 0;
        tank->health = 3;
        tank->alive = true;
    }
    return tank;
}

void destroyTankInstance(Tank* tank) {
    if (tank) free(tank);
}

void setTankPosition(Tank* tank, int x, int y) {
    if (tank) {
        tank->rect.x = x;
        tank->rect.y = y;
    }
}

void setTankAngle(Tank* tank, float angle) {
    if (tank) tank->angle = angle;
}

void setTankHealth(Tank* tank, int health) {
    if (tank) {
        tank->health = health;
        tank->alive = (health > 0);
    }
}

bool isTankAlive(Tank* tank) {
    return tank ? tank->alive : false;
}

int getTankHealth(const Tank* tank) {
    return tank->health;
}

SDL_Rect getTankRect(Tank* tank) {
    if (tank) return tank->rect;
    return (SDL_Rect){0, 0, 0, 0};
}

float getTankAngle(Tank* tank) {
    return tank ? tank->angle : 0.0f;
}

void initTank(SDL_Renderer* renderer) {
    tankTexture = IMG_LoadTexture(renderer, "../lib/resources/tank.png");
    if (!tankTexture) {
        SDL_Log("Kunde inte ladda tank.png: %s", IMG_GetError());
    }
}

void drawTank(SDL_Renderer* renderer, Tank* tank, SDL_Texture* tankTexture) {
    if (!tankTexture || !tank) return;
    SDL_RenderCopyEx(renderer, tankTexture, NULL, &tank->rect, tank->angle, NULL, SDL_FLIP_NONE);
}

void loadHeartTexture(SDL_Renderer* renderer) {
    SDL_Surface* surface = IMG_Load("../lib/resources/heart.png");
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

void destroyTank(void) {
    if (tankTexture) {
        SDL_DestroyTexture(tankTexture);
        tankTexture = NULL;
    }
}
