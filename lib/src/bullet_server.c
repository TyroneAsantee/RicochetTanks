#include "bullet_server.h"
#include <math.h>

void initServerBullet(ServerBullet* bullet) {
    bullet->x = 0;
    bullet->y = 0;
    bullet->w = 8;
    bullet->h = 8;
    bullet->velocityX = 0;
    bullet->velocityY = 0;
    bullet->active = false;
    bullet->ownerId = -1;
}

void fireServerBullet(ServerBullet* bullet, float startX, float startY, float angle, int ownerId) {
    bullet->w = 15;
    bullet->h = 15;

    bullet->x = startX - bullet->w / 2;
    bullet->y = startY - bullet->h / 2;
    bullet->velocityX = cosf((angle - 90.0f) * M_PI / 180.0f) * BULLET_SPEED_SERVER;
    bullet->velocityY = sinf((angle - 90.0f) * M_PI / 180.0f) * BULLET_SPEED_SERVER;  
    bullet->active = true;
    bullet->ownerId = ownerId;
    bullet->rect.x = bullet->x;
    bullet->rect.y = bullet->y;
    bullet->rect.w = bullet->w;
    bullet->rect.h = bullet->h;
}


void updateServerBullet(ServerBullet* bullet, float delta_time) {
    if (!bullet->active) return;

    bullet->x += bullet->velocityX * delta_time;
    bullet->y += bullet->velocityY * delta_time;

    if (serverBulletOutOfBounds(bullet)) {
        bullet->active = false;
    }
}

bool serverBulletOutOfBounds(ServerBullet* bullet) {
    return bullet->x < 0 || bullet->x > 800 || bullet->y < 0 || bullet->y > 600;
}
