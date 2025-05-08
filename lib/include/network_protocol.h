#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <stdbool.h>

#define MAX_PLAYERS 4

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
    ClientCommand command;
    int playerNumber;
    float angle;
    bool up;
    bool down;
    bool shooting;
    int tankColorId;
} ClientData;

typedef struct {
    int playerNumber;
    float x;
    float y;
    float angle;
    bool shooting;
    int tankColorId;
} TankState;

typedef struct {
    ServerCommand command;
    int numPlayers;
    TankState tanks[MAX_PLAYERS];
} ServerData;

typedef struct {
    ServerCommand command;
    int playerID;
    int arenaWidth;
    int arenaHeight;
} GameInitData;
#pragma pack(pop)

#endif 