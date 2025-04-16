#include "bullet.h"
#include <math.h>
#include <SDL2/SDL_image.h>

static SDL_Texture* bulletTexture = NULL;

void loadBulletTexture(SDL_Renderer* renderer) 
{
    bulletTexture = IMG_LoadTexture(renderer, "resources/bullet.png");
    if (!bulletTexture) 
    {
        SDL_Log("Kunde inte ladda bullet.png: %s", IMG_GetError());
    }
}

void destroyBulletTexture() 
{
    if (bulletTexture) 
    {
        SDL_DestroyTexture(bulletTexture);
        bulletTexture = NULL;
    }
}

void initBullet(Bullet* bullet) 
{
    bullet->rect.x = 0;
    bullet->rect.y = 0;
    bullet->rect.w = 8;
    bullet->rect.h = 8;
    bullet->velocityX = 0;
    bullet->velocityY = 0;
    bullet->active = false;
}

void fireBullet(Bullet* bullet, float startX, float startY, float angle) 
{
    float radians = (angle - 90.0f) * M_PI / 180.0f;
    bullet->rect.x = startX;
    bullet->rect.y = startY;
    bullet->rect.w = 18;  // Skala ner från 360x360 
    bullet->rect.h = 18;
    bullet->velocityX = cos(radians) * BULLET_SPEED;
    bullet->velocityY = sin(radians) * BULLET_SPEED;
    bullet->active = true;
}

void updateBullet(Bullet* bullet, float delta_time) 
{
    if (!bullet->active) return;

    bullet->rect.x += bullet->velocityX * delta_time;
    bullet->rect.y += bullet->velocityY * delta_time;

    if (bulletOutOfBounds(bullet)) 
    {
        bullet->active = false;
    }
}

void renderBullet(SDL_Renderer* renderer, Bullet* bullet) 
{
    if (!bullet->active || !bulletTexture) return;

    SDL_RenderCopyF(renderer, bulletTexture, NULL, &bullet->rect);
}

bool bulletOutOfBounds(Bullet* bullet) 
{
    return (bullet->rect.x < 0 || bullet->rect.x > 800 ||
            bullet->rect.y < 0 || bullet->rect.y > 600);
}
