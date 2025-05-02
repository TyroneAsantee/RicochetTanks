#include <stdlib.h>
#include <stdbool.h>
#include "wall.h"

struct Wall {
    SDL_Rect vertical;
    SDL_Rect horizontal;
};

Wall* createWall(int x, int y, int thickness, int length, WallDirection dir) {
    Wall* wall = (Wall*)malloc(sizeof(Wall));
    if (!wall) return NULL;

    switch (dir) {
        case WALL_TOP_LEFT:
            wall->vertical = (SDL_Rect){x, y, thickness, length};
            wall->horizontal = (SDL_Rect){x, y, length, thickness};
            break;
        case WALL_TOP_RIGHT:
            wall->vertical = (SDL_Rect){x + length - thickness, y, thickness, length};
            wall->horizontal = (SDL_Rect){x, y, length, thickness};
            break;
        case WALL_BOTTOM_LEFT:
            wall->vertical = (SDL_Rect){x, y, thickness, length};
            wall->horizontal = (SDL_Rect){x, y + length - thickness, length, thickness};
            break;
        case WALL_BOTTOM_RIGHT:
            wall->vertical = (SDL_Rect){x + length - thickness, y, thickness, length};
            wall->horizontal = (SDL_Rect){x, y + length - thickness, length, thickness};
            break;
    }

    return wall;
}

void renderWall(SDL_Renderer* renderer, Wall* wall) {
    SDL_SetRenderDrawColor(renderer, 0, 180, 220, 255);
    SDL_RenderFillRect(renderer, &wall->vertical);
    SDL_RenderFillRect(renderer, &wall->horizontal);
}

int wallCheckCollision(Wall* wall, SDL_Rect* player) {
    return SDL_HasIntersection(&wall->vertical, player) ||
           SDL_HasIntersection(&wall->horizontal, player);
}

bool wallHitsVertical(Wall* topLeft, Wall* topRight, Wall* bottomLeft, Wall* bottomRight, SDL_Rect* bullet) {
    return SDL_HasIntersection(&topLeft->vertical, bullet) ||
           SDL_HasIntersection(&topRight->vertical, bullet) ||
           SDL_HasIntersection(&bottomLeft->vertical, bullet) ||
           SDL_HasIntersection(&bottomRight->vertical, bullet);
}

bool wallHitsHorizontal(Wall* topLeft, Wall* topRight, Wall* bottomLeft, Wall* bottomRight, SDL_Rect* bullet) {
    return SDL_HasIntersection(&topLeft->horizontal, bullet) ||
           SDL_HasIntersection(&topRight->horizontal, bullet) ||
           SDL_HasIntersection(&bottomLeft->horizontal, bullet) ||
           SDL_HasIntersection(&bottomRight->horizontal, bullet);
}

void destroyWall(Wall* wall) {
    if (wall) {
        free(wall);
    }
}