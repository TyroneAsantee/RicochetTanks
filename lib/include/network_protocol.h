#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <stdbool.h>

#define MAX_PLAYERS 4
#define MAX_BULLETS 20

typedef enum {
    CONNECT,
    UPDATE,
    HEARTBEAT
} ClientCommand;

typedef enum {
    START_MATCH,
    GAME_STATE
} ServerCommand;

#pragma pack(push, 1)

typedef struct {
    float x;
    float y;
    float vx;
    float vy;
    bool active;
    int ownerId;
} BulletState;

typedef struct {
    float x;
    float y;
    float angle;
    bool connected;
    int health;
} PlayerState;

typedef struct {
    ClientCommand command;
    float x;
    float y;
    float angle;
    bool fire;
} ClientMessage;

typedef struct {
    ServerCommand command;
    PlayerState players[MAX_PLAYERS];
    BulletState bullets[MAX_BULLETS];
} ServerMessage;

// ✳️ SAKNADE STRUKTURER — Dessa används i main.c
typedef struct {
    ClientCommand command;
    int playerNumber;
    int tankColorId;
    bool up, down, left, right, shooting;
    float angle;
} ClientData;

typedef struct {
    int playerNumber;
    int x, y;
    float angle;
    int tankColorId;
    int health;
    bool shooting;
} TankState;

typedef struct {
    ServerCommand command;
    int numPlayers;
    TankState tanks[MAX_PLAYERS];
    BulletState bullets[MAX_BULLETS];
    int numBullets;
} ServerData;

typedef struct {
    ServerCommand command;
    int playerID;
    int arenaWidth;
    int arenaHeight;
} GameInitData;

#pragma pack(pop)

#endif
