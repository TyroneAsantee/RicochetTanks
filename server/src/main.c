#include <SDL2/SDL.h>
#include <SDL2/SDL_net.h>
#include <string.h>
#include "tank_server.h"
#include "network_protocol.h"
#include "wall.h"

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
Wall* topLeftWall;
Wall* topRightWall;
Wall* bottomLeftWall;
Wall* bottomRightWall;

bool initServer();
void sendInitialGameData(Player *player);
void handleClientConnections(float dt);
void broadcastGameState();
void checkPlayerHeartbeats();
void updateTanks(float dt);
int serverThread(void* data);

int main(int argc, char* argv[]) {
    Uint32 lastUpdate = SDL_GetTicks();
    if (!initServer()) {
        return -1;
    }

    Uint32 lastBroadcast = SDL_GetTicks();
    numConnectedPlayers = 0;

    while (true) {
        Uint32 now = SDL_GetTicks();
        float dt = (now - lastUpdate) / 1000.0f; 
        if (dt > 0.05f) dt = 0.05f;

        handleClientConnections(dt);  
        checkPlayerHeartbeats();

        if (now - lastBroadcast > 100) {
            updateTanks(dt);
            broadcastGameState();
            lastBroadcast = now;
        }

        lastUpdate = now;
        SDL_Delay(10);
    }
    
    destroyWall(topLeftWall);
    destroyWall(topRightWall);
    destroyWall(bottomLeftWall);
    destroyWall(bottomRightWall);

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

    int thickness = 20;
    int length = 80;

    topLeftWall = createWall(100, 100, thickness, length, WALL_TOP_LEFT);
    topRightWall = createWall(WINDOW_WIDTH - 100 - length, 100, thickness, length, WALL_TOP_RIGHT);
    bottomLeftWall = createWall(100, WINDOW_HEIGHT - 100 - length, thickness, length, WALL_BOTTOM_LEFT);
    bottomRightWall = createWall(WINDOW_WIDTH - 100 - length, WINDOW_HEIGHT - 100 - length, thickness, length, WALL_BOTTOM_RIGHT);

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

    SDLNet_UDP_Send(serverSocket, -1, initPacket);

    SDLNet_FreePacket(initPacket);
}

void handleClientConnections(float dt) {
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
            int margin = 10;
            int tankW = 64, tankH = 64; // eller hämta från getTankRect()
            int length = 80;

            int x = 0, y = 0;

            switch (index) {
                case 0: // top left
                    x = 100 + length + margin;
                    y = 100 + length + margin;
                    break;
                case 1: // top right
                    x = WINDOW_WIDTH - 100 - length - tankW - margin;
                    y = 100 + length + margin;
                    break;
                case 2: // bottom left
                    x = 100 + length + margin;
                    y = WINDOW_HEIGHT - 100 - length - tankH - margin;
                    break;
                case 3: // bottom right
                    x = WINDOW_WIDTH - 100 - length - tankW - margin;
                    y = WINDOW_HEIGHT - 100 - length - tankH - margin;
                    break;
                default:
                    x = 400; y = 300; // fallback om något blir knas
                    break;
            }

            setTankPosition(tank, x, y);
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

            SDLNet_UDP_Send(serverSocket, -1, packet);

            sendInitialGameData(&newPlayer);
            SDL_Delay(100);
            broadcastGameState();

        } else if (request.command == UPDATE) {
            int id = request.playerNumber;
            if (id >= 1 && id <= MAX_PLAYERS) {
                int i = id - 1;
                playerStatus[i].up = request.up;
                playerStatus[i].down = request.down;
                playerStatus[i].left = request.left;
                playerStatus[i].right = request.right;
                playerStatus[i].angle = request.angle;
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
            SDL_Log("204Tank %d: x=%d y=%d angle=%.2f", i + 1, rect.x, rect.y, getTankAngle(tanks[i]));
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

void updateTanks(float dt) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!connectedPlayers[i].active || !tanks[i]) continue;

        float angle = playerStatus[i].angle;

        // Rotation
        if (playerStatus[i].left && !playerStatus[i].right) {
            angle -= 10.0f;
        } else if (playerStatus[i].right && !playerStatus[i].left) {
            angle += 10.0f;
        }

        setTankAngle(tanks[i], angle);

        // Rörelse
        float speed = 800.0f;
        float radians = (angle - 90.0f) * M_PI / 180.0f;
        float dx = 0, dy = 0;

        if (playerStatus[i].up && !playerStatus[i].down) {
            dx = cosf(radians) * speed * dt;
            dy = sinf(radians) * speed * dt;
        } else if (playerStatus[i].down && !playerStatus[i].up) {
            dx = -cosf(radians) * speed * dt;
            dy = -sinf(radians) * speed * dt;
        }

        SDL_Rect rect = getTankRect(tanks[i]);
        rect.x += dx;
        rect.y += dy;

        // Begränsningar
        if (rect.x < 0) rect.x = 0;
        if (rect.y < 0) rect.y = 0;
        if (rect.x > WINDOW_WIDTH - rect.w) rect.x = WINDOW_WIDTH - rect.w;
        if (rect.y > WINDOW_HEIGHT - rect.h) rect.y = WINDOW_HEIGHT - rect.h;

        if (!wallCheckCollision(topLeftWall, &rect) &&
            !wallCheckCollision(topRightWall, &rect) &&
            !wallCheckCollision(bottomLeftWall, &rect) &&
            !wallCheckCollision(bottomRightWall, &rect)) {
            setTankPosition(tanks[i], rect.x, rect.y);
        } else {
            // Eventuellt logga kollision
            SDL_Log("Tank %d krockade med vägg.", i + 1);
        }
    }
}


