#ifndef GAME_H
#define GAME_H

#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include "timer.h"
#include "tank.h"
#include "bullet.h"
#include "wall.h"

typedef enum {
    STATE_MENU,
    STATE_RUNNING,
    STATE_SELECT_TANK,
    STATE_EXIT
} GameState;

typedef struct {
    SDL_Window *pWindow;
    SDL_Renderer *pRenderer;
    SDL_Texture *pBackground;
    SDL_Texture *pTankpicture;
    SDL_Event event;
    float angle;
    Timer timer;
    Tank* tank;
    Bullet bullets[MAX_BULLETS];
    UDPsocket pSocket;
    IPaddress serverAddress;
    UDPpacket *pPacket;
    int playerNumber;
    int tankColorId;
    int bulletstopper;
    int lastshottime;
    char ipAddress[64];
    Wall* topLeft;
    Wall* topRight;
    Wall* bottomLeft;
    Wall* bottomRight;
    GameState state;
} Game;

#endif