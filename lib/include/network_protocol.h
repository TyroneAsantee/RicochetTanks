#ifndef NETWORK_PROTOCOL_H
#define NETWORK_PROTOCOL_H

#include <stdbool.h>

#define MAX_PLAYERS 4

typedef enum {
    CONNECT,    // Klient vill ansluta
    UPDATE,     // Klient skickar psotion/data
    HEARTBEAT,  // Klienten håller sig vid liv
    START_MATCH,    // Host startar spelet
    GAME_STATE      // Server skickar spelstatus
} ClientCommand;

typedef struct {
    ClientCommand command;
    int playerNumber;
    float x;
    float y;
    float angle;
    bool shooting;
} ClientData;

typedef struct {
    int playerNumber;
    float x;
    float y;
    float angle;
    bool shooting;
} TankState;

typedef struct {
    ClientCommand command;
    int numPlayers;
    TankState tanks[MAX_PLAYERS];
} ServerData;

#endif 