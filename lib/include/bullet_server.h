#ifndef BULLET_SERVER_H
#define BULLET_SERVER_H

#include <stdbool.h>
#include <SDL.h>

#define BULLET_SPEED_SERVER 1500
#define MAX_BULLETS 20

typedef struct {
    float x, y;
    float velocityX;
    float velocityY;
    float w, h;
    bool active;
    int ownerId;
    SDL_FRect rect;
} ServerBullet;

void initServerBullet(ServerBullet* bullet);
void fireServerBullet(ServerBullet* bullet, float startX, float startY, float angle, int ownerId);
void updateServerBullet(ServerBullet* bullet, float delta_time);
bool serverBulletOutOfBounds(ServerBullet* bullet);

#endif
