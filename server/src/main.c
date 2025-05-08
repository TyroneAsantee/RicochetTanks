#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <string.h>
#include "tank_server.h"
#include "network_protocol.h"

#define SERVER_PORT 12345
#define MAX_PLAYERS 4
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

static Player connectedPlayers[MAX_PLAYERS];
static PlayerStatus playerStatus[MAX_PLAYERS];
int numConnectedPlayers = 0;
static Tank* tanks[MAX_PLAYERS];
static UDPsocket serverSocket;
static UDPpacket *packet;

bool initServer();
void sendInitialGameData(Player *player);
void handleClientConnections();
void broadcastGameState();
void checkPlayerHeartbeats();
int serverThread(void* data);

int main(int argc, char* argv[]) {
    if (!initServer()) {
        return -1;
    }

    Uint32 lastBroadcast = SDL_GetTicks();
    numConnectedPlayers = 0;
    SDL_Log("INFO: sizeof(ClientData) = %lu", sizeof(ClientData));

    while (true) {
        handleClientConnections();
        checkPlayerHeartbeats();

        Uint32 now = SDL_GetTicks();
        if (now - lastBroadcast > 100) {
            broadcastGameState();
            lastBroadcast = now;
        }
        SDL_Delay(10);
    }

    return 0;
}


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
    UDPpacket *initPacket = SDLNet_AllocPacket(512);
    if (!initPacket) {
        SDL_Log("ERROR: Kunde inte allokera nytt paket för GameInitData");
        return;
    }

    GameInitData initData = {
        .command = START_MATCH,
        .playerID = player->playerID,
        .arenaWidth = WINDOW_WIDTH,
        .arenaHeight = WINDOW_HEIGHT
    };

    initPacket->address = player->address;
    memcpy(initPacket->data, &initData, sizeof(GameInitData));
    initPacket->len = sizeof(GameInitData);

    SDL_Log("84DEBUG: Förbereder att skicka GameInitData (START_MATCH) till spelare %d", player->playerID);
    SDLNet_UDP_Send(serverSocket, -1, initPacket);

    SDLNet_FreePacket(initPacket);
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
                index + 1,
                true
            };

            connectedPlayers[index] = newPlayer;

            Tank* tank = createTank();
            if (!tank) {
                SDL_Log("ERROR: Kunde inte skapa tank för spelare %d", index + 1);
                continue;
            }
            setTankPosition(tank, 100 + 100 * index, 100);
            setTankColorId(tank, request.tankColorId);
            tanks[index] = tank;

            SDL_Log("127INFO: Tank %d skapad med färg %d på position (%d, %d)", index + 1, request.tankColorId, 100 + 100 * index, 100);

            playerStatus[index].lastHeartbeat = SDL_GetTicks();
            playerStatus[index].active = true;
            numConnectedPlayers++;

            SDL_Log("New player connected. ID: %d, total players: %d", newPlayer.playerID, numConnectedPlayers);

            ClientData response = { CONNECT };
            response.playerNumber = newPlayer.playerID;

            memcpy(packet->data, &response, sizeof(ClientData));
            packet->len = sizeof(ClientData);
            packet->address = newPlayer.address;

            SDL_Log("139DEBUG: Förbereder att skicka CONNECT-svar till klient");
            for (int i = 0; i < packet->len; i++) {
                SDL_Log("Byte %d: %02X", i, packet->data[i]);
            }
            SDLNet_UDP_Send(serverSocket, -1, packet);

            sendInitialGameData(&newPlayer);
        } else if (request.command == UPDATE) {
            int id = request.playerNumber;
            if (id >= 1 && id <= MAX_PLAYERS) {
                int i = id - 1;

                if (tanks[i]) {
                    float dt = 0.1f;
                    float speed = 100.0f;
                    float angleRad = request.angle * M_PI / 180.0f;
                    float dx = cosf(angleRad) * speed * dt;
                    float dy = sinf(angleRad) * speed * dt;

                    SDL_Rect rect = getTankRect(tanks[i]);

                    SDL_Log("UPDATE från spelare %d – vinkel: %.2f | up: %d | down: %d",
                            request.playerNumber, request.angle, request.up, request.down);

                    if (request.up) {
                        setTankPosition(tanks[i], rect.x + dx, rect.y + dy);
                    }
                    if (request.down) {
                        setTankPosition(tanks[i], rect.x - dx, rect.y - dy);
                    }

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

    int activeTankCount = 0;
    for (int i = 0; i < numConnectedPlayers; i++) {
        if (tanks[i] && connectedPlayers[i].active) {
            gameState.tanks[activeTankCount].playerNumber = connectedPlayers[i].playerID;
            SDL_Rect rect = getTankRect(tanks[i]);
            gameState.tanks[activeTankCount].x = rect.x;
            gameState.tanks[activeTankCount].y = rect.y;
            gameState.tanks[activeTankCount].angle = getTankAngle(tanks[i]);
            gameState.tanks[activeTankCount].shooting = false;
            gameState.tanks[activeTankCount].tankColorId = getTankColorId(tanks[i]);
            SDL_Log("Tank %d: x=%d y=%d angle=%.2f", i + 1, rect.x, rect.y, getTankAngle(tanks[i]));
            activeTankCount++;
        }
    }
    gameState.numPlayers = activeTankCount;

    SDL_Log("Broadcasting to %d active players", activeTankCount);

    for (int i = 0; i < numConnectedPlayers; i++) {
        if (!connectedPlayers[i].active) continue;

        packet->address = connectedPlayers[i].address;
        memcpy(packet->data, &gameState, sizeof(ServerData));
        packet->len = sizeof(ServerData);

        SDL_Log("Rad208: DEBUG: Skickar ServerData med len=%d (ska vara %zu)", packet->len, sizeof(ServerData));
        SDLNet_UDP_Send(serverSocket, -1, packet);
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


