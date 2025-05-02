#ifndef WALL_H
#define WALL_H

#include <SDL.h>

typedef enum {
    WALL_TOP_LEFT,
    WALL_TOP_RIGHT,
    WALL_BOTTOM_LEFT,
    WALL_BOTTOM_RIGHT
} WallDirection;

typedef struct Wall Wall;

Wall* createWall(int x, int y, int thickness, int length, WallDirection dir);

void renderWall(SDL_Renderer* renderer, Wall* wall);

int wallCheckCollision(Wall* wall, SDL_Rect* player);

bool wallHitsVertical(Wall* topLeft, Wall* topRight, Wall* bottomLeft, Wall* bottomRight, SDL_Rect* bullet);

bool wallHitsHorizontal(Wall* topLeft, Wall* topRight, Wall* bottomLeft, Wall* bottomRight, SDL_Rect* bullet);

void destroyWall(Wall* wall);

#endif