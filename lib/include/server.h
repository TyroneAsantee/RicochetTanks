#ifndef SERVER_H
#define SERVER_H

#include <SDL2/SDL_net.h>
#include <SDL2/SDL.h>
#include <stdbool.h>
#include "network_protocol.h"

#define SERVER_PORT 12345
#define MAX_PLAYERS 4

typedef struct {
    IPaddress address;
    int playerID;
    bool active;
} Player;

typedef struct {
    Uint32 lastHeartbeat;
    bool active;
} PlayerStatus;

bool initServer(void);
void handleClientConnections(void);
void checkPlayerHeartbeats(void);
int serverThread(void* data);

#endif 