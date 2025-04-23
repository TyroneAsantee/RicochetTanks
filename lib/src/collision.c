#include "collision.h"

bool checkCollision(SDL_Rect* a, SDL_FRect* b) {
    return !(a->x + a->w < b->x ||
             a->x > b->x + b->w ||
             a->y + a->h < b->y ||
             a->y > b->y + b->h);
}