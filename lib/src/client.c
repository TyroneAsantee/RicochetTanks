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
    IPaddress serverIP;
    if (SDLNet_ResolveHost(&serverIP, ip, SERVER_PORT) == -1) {
        SDL_Log("SDLNet_ResolveHost: %s", SDLNet_GetError());
        return false;
    }

    game->pSocket = SDLNet_UDP_Open(0);
    if (!game->pSocket) {
        SDL_Log("SDLNet_UDP_Open: %s", SDLNet_GetError());
        return false;
    }

    game->pPacket = SDLNet_AllocPacket(512);
    if (!game->pPacket) {
        SDL_Log("SDLNet_AllocPacket: %s", SDLNet_GetError());
        return false;
    }

    ClientData request = { CONNECT };
    request.tankColorId = game->tankColorId;

    memcpy(game->pPacket->data, &request, sizeof(ClientData));
    game->pPacket->len = sizeof(ClientData);
    game->pPacket->address = serverIP;
    SDLNet_UDP_Send(game->pSocket, -1, game->pPacket);

    Uint32 start = SDL_GetTicks();
    *timedOut = false;

    while (SDL_GetTicks() - start < 3000) {
        if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
            ClientData response;
            memcpy(&response, game->pPacket->data, sizeof(ClientData));
            
            if (response.command == CONNECT) {
                game->playerNumber = response.playerNumber; 
                SDL_Log("INFO: Connected as player %d", game->playerNumber);
                return true;
            }
        }
        SDL_Delay(10);
    }

    *timedOut = true;
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

    if (numready > 0 && SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
        ClientData response;
        memcpy(&response, game->pPacket->data, sizeof(ClientData));
        if (response.command == CONNECT) {
            game->playerNumber = response.playerNumber;
            SDL_Log("Connected as player %d", game->playerNumber);
            SDLNet_FreeSocketSet(socketSet);
            return true;
        }
    }

    SDLNet_FreeSocketSet(socketSet);
    return false;
}

void receiveGameState(Game* game) {
    if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
        ServerData serverData;
        SDL_Log("DEBUG: Mottaget paket med längd: %d (förväntat: %lu)", game->pPacket->len, sizeof(ServerData));

        memcpy(&serverData, game->pPacket->data, game->pPacket->len < sizeof(ServerData) ? game->pPacket->len : sizeof(ServerData));

        if (serverData.command == GAME_STATE) {
            SDL_Log("DEBUG: serverData.command == GAME_STATE");
            game->numOtherTanks = 0;

            for (int i = 0; i < serverData.numPlayers; i++) {
                SDL_Log("DEBUG: Kollar tank %d: playerNumber=%d (jag är %d)", i, serverData.tanks[i].playerNumber, game->playerNumber);
                if (serverData.tanks[i].playerNumber == game->playerNumber) {
                    if (!game->tank) {
                        game->tank = createTank();
                        SDL_Log("INFO: Klientens tank skapad (playerNumber=%d)", game->playerNumber);
                    }
                    SDL_Log("DEBUG: Uppdaterar tankens position och vinkel");
                    setTankPosition(game->tank, serverData.tanks[i].x, serverData.tanks[i].y);
                    setTankAngle(game->tank, serverData.tanks[i].angle);
                    setTankColorId(game->tank, serverData.tanks[i].tankColorId);
                } else {
                    game->otherTanks[game->numOtherTanks++] = serverData.tanks[i];
                }
            }
        } else {
            SDL_Log("DEBUG: serverData.command != GAME_STATE (%d)", serverData.command);
        }
    }
}