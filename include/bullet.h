#ifndef BULLET_H
#define BULLET_H
#include <SDL2/SDL.h>
#include <stdbool.h>
#define BULLET_SPEED 300
#define MAX_BULLETS 20

typedef struct {
    SDL_FRect rect;
    float velocityX;
    float velocityY;
    bool active;
} Bullet;

void loadBulletTexture(SDL_Renderer* renderer);
void destroyBulletTexture(void);
void initBullet(Bullet* bullet);
void fireBullet(Bullet* bullet, float startX, float startY, float angle);
void updateBullet(Bullet* bullet, float delta_time);
void renderBullet(SDL_Renderer* renderer, Bullet* bullet);
bool bulletOutOfBounds(Bullet* bullet);

#endif
