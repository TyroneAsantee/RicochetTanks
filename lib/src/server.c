#include "server.h"
#include <string.h>
#include "tank_server.h"
#include "network_protocol.h"


#define WINDOW_WIDTH 800 
#define WINDOW_HEIGHT 600


static Player connectedPlayers[MAX_PLAYERS];
static PlayerStatus playerStatus[MAX_PLAYERS];
int numConnectedPlayers = 0;
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
            int index = -1;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (!connectedPlayers[i].active) {
                    index = i;
                    break;
                }
            }

            if (index == -1) {
                SDL_Log("Server full – kunde inte tilldela plats.");
                continue;
            }

            Player newPlayer = {
                packet->address,
                index + 1,   // playerID = 1, 2, 3, 4 (ej 0-baserat)
                true
            };

            connectedPlayers[index] = newPlayer;

            Tank* tank = createTank();
            if (!tank) {
                SDL_Log("ERROR: Kunde inte skapa tank för spelare %d", index + 1); // LOGG 
                continue;
            }
            setTankPosition(tank, 100 + 100 * index, 100);
            setTankColorId(tank, request.tankColorId);
            tanks[index] = tank;

            SDL_Log("INFO: Tank %d skapad med färg %d på position (%d, %d)", index + 1, request.tankColorId, 100 + 100 * index, 100); // LOGG

            playerStatus[index].lastHeartbeat = SDL_GetTicks();
            playerStatus[index].active = true;

            numConnectedPlayers++;  // valfri: räknar totala aktiva sessioner

            SDL_Log("New player connected. ID: %d, total players: %d", newPlayer.playerID, numConnectedPlayers);

            // Skicka CONNECT-svar
            ClientData response = {CONNECT};
            response.playerNumber = newPlayer.playerID;

            memcpy(packet->data, &response, sizeof(ClientData));
            packet->len = sizeof(ClientData);
            packet->address = newPlayer.address;
            SDLNet_UDP_Send(serverSocket, -1, packet);

            sendInitialGameData(&newPlayer);
        }

        else if (request.command == UPDATE) {
            int id = request.playerNumber;
            if (id >= 1 && id <= MAX_PLAYERS) {
                int i = id - 1;

                if (tanks[i]) {
                    setTankPosition(tanks[i], request.x, request.y);
                    setTankAngle(tanks[i], request.angle);
                }
                playerStatus[i].lastHeartbeat = SDL_GetTicks();
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
            gameState.tanks[i].tankColorId = getTankColorId(tanks[i]);
            SDL_Log("Tank %d: x=%d y=%d angle=%.2f", i + 1, rect.x, rect.y, getTankAngle(tanks[i])); //LOGG
        }
    }
    int activePlayers = 0;
    for (int i = 0; i < numConnectedPlayers; i++) {
        if (connectedPlayers[i].active)
            activePlayers++;
    }
    SDL_Log("Broadcasting to %d active players", activePlayers);

    for (int i = 0; i < numConnectedPlayers; i++) {
        if (!connectedPlayers[i].active) continue;

        packet->address = connectedPlayers[i].address;
        memcpy(packet->data, &gameState, sizeof(ServerData));
        packet->len = sizeof(ServerData);
        SDLNet_UDP_Send(serverSocket, i, packet);
        SDL_Log("Sent state to player %d", connectedPlayers[i].playerID);
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
    numConnectedPlayers = 0;

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