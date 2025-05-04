#include "server.h"
#include <string.h>
#include "tank.h"

#define WINDOW_WIDTH 800 
#define WINDOW_HEIGHT 600


static Player connectedPlayers[MAX_PLAYERS];
static PlayerStatus playerStatus[MAX_PLAYERS];
static int numConnectedPlayers = 1;
static Tank* tanks[MAX_PLAYERS];

static UDPsocket serverSocket;
static UDPpacket *packet;

typedef struct {
    int playerID;
    int arenaWidth;
    int arenaHeight;
} GameInitData;

typedef enum {
    CONNECT, 
    UPDATE,
 } ClientCommand;

typedef struct {
    ClientCommand command;
    int playerNumber;
    Tank* tank;
 } ClientData;

bool initServer() {
    if (SDLNet_Init() == -1) {
        SDL_Log("SDLNet_Init: %s", SDLNet_GetError());
        return false;
    }

    serverSocket = SDLNet_UDP_Open(SERVER_PORT);
    if (!serverSocket) {
        SDL_Log("SDLNet_UDP_Open: %s", SDLNet_GetError());
        return false;
    }

    packet = SDLNet_AllocPacket(512);
    if (!packet) {
        SDL_Log("SDLNet_AllocPacket: %s", SDLNet_GetError());
        return false;
    }

    return true;
}

void sendInitialGameData(Player *player) {
  
    GameInitData initData = {
        player->playerID, WINDOW_WIDTH, WINDOW_HEIGHT,
    };

    packet->address = player->address;
    memcpy(packet->data, &initData, sizeof(GameInitData));
    packet->len = sizeof(GameInitData);
    SDLNet_UDP_Send(serverSocket, -1, packet);
}

void handleClientConnections() {
    if (numConnectedPlayers < MAX_PLAYERS && SDLNet_UDP_Recv(serverSocket, packet)) {
        ClientData request;
        memcpy(&request, packet->data, sizeof(ClientData));

        if (request.command == CONNECT) {
            Player newPlayer = {
                packet->address,
                numConnectedPlayers + 1,
                true
            };
            Tank* tank = createTank();
            tanks[numConnectedPlayers - 1] = tank;

            connectedPlayers[numConnectedPlayers] = newPlayer;
            playerStatus[numConnectedPlayers] = (PlayerStatus){SDL_GetTicks(), true};
            numConnectedPlayers++;

            ClientData response = {CONNECT, newPlayer.playerID};
            memcpy(packet->data, &response, sizeof(ClientData));
            packet->len = sizeof(ClientData);

            SDLNet_UDP_Send(serverSocket, -1, packet);
            SDL_Log("Player %d connected", newPlayer.playerID);

            sendInitialGameData(&newPlayer);
        }
        if(SDLNet_UDP_Recv(serverSocket, packet) && request.command == UPDATE){
            // int playerid = packet.id;
            // run();
        }
    }
}

void checkPlayerHeartbeats() {
    Uint32 currentTime = SDL_GetTicks();
    for (int i = 0; i < numConnectedPlayers; i++) {
        if (playerStatus[i].active && (currentTime - playerStatus[i].lastHeartbeat > 5000)) {
            playerStatus[i].active = false;
            connectedPlayers[i].active = false;
            SDL_Log("Player %d disconnected due to timeout.", connectedPlayers[i].playerID);
        }
    }
}

int serverThread(void* data) {
    if (!initServer()) {
        return -1;
    }

    while (true) {
        handleClientConnections();
        checkPlayerHeartbeats();
        SDL_Delay(10);
    }

    return 0;
}