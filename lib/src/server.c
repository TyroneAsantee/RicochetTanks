#include "server.h"
#include <string.h>
#include "tank.h"
#include "network_protocol.h"


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
    while (SDLNet_UDP_Recv(serverSocket, packet)) {
        ClientData request;
        memcpy(&request, packet->data, sizeof(ClientData));

        if (request.command == CONNECT && numConnectedPlayers < MAX_PLAYERS) {
            Player newPlayer = {
                packet->address,
                numConnectedPlayers + 1,
                true
            };

            Tank* tank = createTank();
            setTankPosition(tank, 100 + 100 * numConnectedPlayers, 100);  // exempelposition
            tanks[numConnectedPlayers] = tank;

            connectedPlayers[numConnectedPlayers] = newPlayer;
            playerStatus[numConnectedPlayers] = (PlayerStatus){SDL_GetTicks(), true};
            numConnectedPlayers++;

            ClientData response = {CONNECT, newPlayer.playerID};
            memcpy(packet->data, &response, sizeof(ClientData));
            packet->len = sizeof(ClientData);
            packet->address = newPlayer.address;

            SDLNet_UDP_Send(serverSocket, -1, packet);
            SDL_Log("Player %d connected", newPlayer.playerID);

            sendInitialGameData(&newPlayer);
        }

        else if (request.command == UPDATE) {
            int id = request.playerNumber;
            if (id >= 1 && id <= MAX_PLAYERS) {
                // Uppdatera tankens data på servern
                if (tanks[id - 1]) {
                    setTankPosition(tanks[id - 1], request.x, request.y);
                    setTankAngle(tanks[id - 1], request.angle);
                }
                playerStatus[id - 1].lastHeartbeat = SDL_GetTicks();
            }
        }
    }
}

void broadcastGameState() {
    ServerData gameState;
    gameState.command = GAME_STATE;
    gameState.numPlayers = numConnectedPlayers;

    for (int i = 0; i < numConnectedPlayers; i++) {
        if (tanks[i] && connectedPlayers[i].active) {
            gameState.tanks[i].playerNumber = connectedPlayers[i].playerID;
            SDL_Rect rect = getTankRect(tanks[i]);
            gameState.tanks[i].x = rect.x;
            gameState.tanks[i].y = rect.y;
            gameState.tanks[i].angle = getTankAngle(tanks[i]);
            gameState.tanks[i].shooting = false; // TODO: Skottlogik
        }
    }

    for (int i = 0; i < numConnectedPlayers; i++) {
        if (!connectedPlayers[i].active) continue;

        packet->address = connectedPlayers[i].address;
        memcpy(packet->data, &gameState, sizeof(ServerData));
        packet->len = sizeof(ServerData);
        SDLNet_UDP_Send(serverSocket, -1, packet);
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
   
   Uint32 lastBroadcast = SDL_GetTicks();

    while (true) {
        handleClientConnections();
        checkPlayerHeartbeats();

        Uint32 now = SDL_GetTicks();
        if (now - lastBroadcast > 100) { // var 100ms
            broadcastGameState();
            lastBroadcast = now;
        }

        SDL_Delay(10);
    }
    return 0;
}