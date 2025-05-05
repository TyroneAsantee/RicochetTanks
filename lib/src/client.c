#include "client.h"
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_net.h>
#include <SDL2/SDL_ttf.h>
#include <string.h>
#include "text.h"
#include "game.h"
#include "network_protocol.h"

#define SERVER_PORT 12345

bool connectToServer(Game* game, const char* ip, bool *timedOut) {
    *timedOut = false;

    if (SDLNet_Init() != 0) {
        SDL_Log("SDLNet_Init failed: %s", SDLNet_GetError());
        return false;
    }

    game->pSocket = SDLNet_UDP_Open(0);
    if (!game->pSocket) {
        SDL_Log("SDLNet_UDP_Open failed: %s", SDLNet_GetError());
        SDLNet_Quit();
        return false;
    }

    if (SDLNet_ResolveHost(&game->serverAddress, ip, SERVER_PORT) != 0) {
        SDL_Log("SDLNet_ResolveHost failed for %s: %s", ip, SDLNet_GetError());
        SDLNet_UDP_Close(game->pSocket);
        SDLNet_Quit();
        return false;
    }

    game->pPacket = SDLNet_AllocPacket(512);
    if (!game->pPacket) {
        SDL_Log("SDLNet_AllocPacket failed: %s", SDLNet_GetError());
        SDLNet_UDP_Close(game->pSocket);
        SDLNet_Quit();
        return false;
    }

    ClientData connectData = {CONNECT, 0};
    memcpy(game->pPacket->data, &connectData, sizeof(ClientData));
    game->pPacket->len = sizeof(ClientData);
    game->pPacket->address = game->serverAddress;

    if (SDLNet_UDP_Send(game->pSocket, -1, game->pPacket) == 0) {
        SDL_Log("SDLNet_UDP_Send failed: %s", SDLNet_GetError());
        SDLNet_FreePacket(game->pPacket);
        SDLNet_UDP_Close(game->pSocket);
        SDLNet_Quit();
        return false;
    }

    if (!waitForServerResponse(game, 5000)) {
        SDL_Log("Server connection timeout");
        *timedOut = true;
        SDLNet_FreePacket(game->pPacket);
        SDLNet_UDP_Close(game->pSocket);
        SDLNet_Quit();
        return false;
    }

    if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
        ClientData response;
        memcpy(&response, game->pPacket->data, sizeof(ClientData));
        if (response.command == CONNECT) {
            game->playerNumber = response.playerNumber;
            SDL_Log("Connected as player %d", game->playerNumber);
            return true;
        }
    }

    SDL_Log("No valid response from server");
    SDLNet_FreePacket(game->pPacket);
    SDLNet_UDP_Close(game->pSocket);
    SDLNet_Quit();
    return false;
}

bool waitForServerResponse(Game *game, Uint32 timeout_ms) {
    SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(1);
    if (!socketSet) {
        SDL_Log("SDLNet_AllocSocketSet: %s", SDLNet_GetError());
        return false;
    }

    SDLNet_UDP_AddSocket(socketSet, game->pSocket);
    int numready = SDLNet_CheckSockets(socketSet, timeout_ms);

    SDLNet_FreeSocketSet(socketSet);

    if (numready == -1) {
        SDL_Log("SDLNet_CheckSockets error: %s", SDLNet_GetError());
        return false;
    }

    return numready > 0;
}

