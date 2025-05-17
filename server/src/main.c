#include <SDL.h>
#include <SDL_net.h>
#include <string.h>
#include "tank_server.h"
#include "network_protocol.h"
#include "wall.h"
#include "collision.h"
#include "bullet_server.h"

#define SERVER_PORT 12345
#define MAX_PLAYERS 4
#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAX_BULLETS_PER_PLAYER 5

static Player connectedPlayers[MAX_PLAYERS];
static PlayerStatus playerStatus[MAX_PLAYERS];
static ServerBullet bullets[MAX_PLAYERS * MAX_BULLETS_PER_PLAYER];
int numConnectedPlayers = 0;
static Tank* tanks[MAX_PLAYERS];
static UDPsocket serverSocket;
static UDPpacket *packet;
Wall* topLeftWall;
Wall* topRightWall;
Wall* bottomLeftWall;
Wall* bottomRightWall;


bool initServer();
bool matchStarted = false;
void sendInitialGameData(Player *player);
void handleClientConnections(float dt);
void broadcastGameState();
void checkPlayerHeartbeats();
void updateTanks(float dt);
void updateServerBullets(float dt);
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
            updateServerBullets(dt);  
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
    SDL_Log("Server started");

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
                .address = packet->address,
                .playerID = index + 1,
                .active = true
            };
            connectedPlayers[index] = newPlayer;

            Tank* tank = createTank();
            if (!tank) {
                SDL_Log("ERROR: Kunde inte skapa tank för spelare %d", index + 1);
                continue;
            }

            int margin = 10;
            int tankW = 64, tankH = 64;
            int length = 80;

            int x = 0, y = 0;
            switch (index) {
                case 0: x = 100 + length + margin; y = 100 + length + margin; break;
                case 1: x = WINDOW_WIDTH - 100 - length - tankW - margin; y = 100 + length + margin; break;
                case 2: x = 100 + length + margin; y = WINDOW_HEIGHT - 100 - length - tankH - margin; break;
                case 3: x = WINDOW_WIDTH - 100 - length - tankW - margin; y = WINDOW_HEIGHT - 100 - length - tankH - margin; break;
                default: x = 400; y = 300; break;
            }

            setTankPosition(tank, x, y);
            setTankColorId(tank, request.tankColorId);
            tanks[index] = tank;

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

            
            if (!matchStarted && numConnectedPlayers >= 1) {
                matchStarted = true;
            }

            sendInitialGameData(&connectedPlayers[index]);

            broadcastGameState();
        }

        else if (request.command == UPDATE) {
            int id = request.playerNumber;
            if (id >= 1 && id <= MAX_PLAYERS) {
                int i = id - 1;

                playerStatus[i].up = request.up;
                playerStatus[i].down = request.down;
                playerStatus[i].left = request.left;
                playerStatus[i].right = request.right;
                playerStatus[i].angle = request.angle;
                playerStatus[i].lastHeartbeat = SDL_GetTicks();

                if (request.shooting && tanks[i]) {
                    for (int j = 0; j < MAX_PLAYERS * MAX_BULLETS_PER_PLAYER; j++) {
                        if (!bullets[j].active) {
                            initServerBullet(&bullets[j]);

                            SDL_Rect rect = getTankRect(tanks[i]);
                            float angle = playerStatus[i].angle;
                            float radians = (angle - 90.0f) * M_PI / 180.0f;

                            float centerX = rect.x + rect.w / 2;
                            float centerY = rect.y + rect.h / 2;
                            float muzzleOffset = rect.h / 2 + 10;

                            float startX = centerX + cosf(radians) * muzzleOffset;
                            float startY = centerY + sinf(radians) * muzzleOffset;

                            fireServerBullet(&bullets[j], startX, startY, angle, id);
                            break;
                        }
                    }
                }
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
            gameState.tanks[activeTankCount].health = getTankHealth(tanks[i]);
            activeTankCount++;
        }
    }
    gameState.numPlayers = activeTankCount;

    gameState.numBullets = 0;
    for (int i = 0; i < MAX_PLAYERS * MAX_BULLETS_PER_PLAYER; i++) {
        if (bullets[i].active) {
            gameState.bullets[gameState.numBullets++] = (BulletState){
                .x = bullets[i].x,
                .y = bullets[i].y,
                .vx = bullets[i].velocityX,
                .vy = bullets[i].velocityY,
                .active = bullets[i].active,
                .ownerId = bullets[i].ownerId
            };
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

void updateTanks(float dt) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!connectedPlayers[i].active || !tanks[i]) continue;

        float angle = playerStatus[i].angle;

        if (playerStatus[i].left && !playerStatus[i].right) {
            angle -= 10.0f;
        } else if (playerStatus[i].right && !playerStatus[i].left) {
            angle += 10.0f;
        }

        setTankAngle(tanks[i], angle);

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

        if (rect.x < 0) rect.x = 0;
        if (rect.y < 0) rect.y = 0;
        if (rect.x > WINDOW_WIDTH - rect.w) rect.x = WINDOW_WIDTH - rect.w;
        if (rect.y > WINDOW_HEIGHT - rect.h) rect.y = WINDOW_HEIGHT - rect.h;

        if (!wallCheckCollision(topLeftWall, &rect) &&
            !wallCheckCollision(topRightWall, &rect) &&
            !wallCheckCollision(bottomLeftWall, &rect) &&
            !wallCheckCollision(bottomRightWall, &rect)) {
            setTankPosition(tanks[i], rect.x, rect.y);
        }
    }
}

void updateServerBullets(float dt) {
    for (int i = 0; i < MAX_PLAYERS * MAX_BULLETS_PER_PLAYER; i++) {
        if (!bullets[i].active) continue;

        bullets[i].x += bullets[i].velocityX * dt;
        bullets[i].y += bullets[i].velocityY * dt;

        bullets[i].rect.x = bullets[i].x;
        bullets[i].rect.y = bullets[i].y;

        SDL_Rect bulletRect = {
            (int)bullets[i].x,
            (int)bullets[i].y,
            (int)bullets[i].rect.w,
            (int)bullets[i].rect.h
        };

        if (wallCheckCollision(topLeftWall, &bulletRect) ||
            wallCheckCollision(topRightWall, &bulletRect) ||
            wallCheckCollision(bottomLeftWall, &bulletRect) ||
            wallCheckCollision(bottomRightWall, &bulletRect)) {

            if (wallHitsVertical(topLeftWall, topRightWall, bottomLeftWall, bottomRightWall, &bulletRect)) {
                bullets[i].velocityX *= -1;
            }
            if (wallHitsHorizontal(topLeftWall, topRightWall, bottomLeftWall, bottomRightWall, &bulletRect)) {
                bullets[i].velocityY *= -1;
            }
        }

        for (int j = 0; j < MAX_PLAYERS; j++) {
            if (!connectedPlayers[j].active || !tanks[j]) continue;

            if (bullets[i].ownerId == connectedPlayers[j].playerID) continue;

            SDL_Rect tankRect = getTankRect(tanks[j]);
            if (checkCollision(&tankRect, &bullets[i].rect)) {
                bullets[i].active = false;

                int hp = getTankHealth(tanks[j]);
                if (hp > 0) {
                    setTankHealth(tanks[j], hp - 1);
                }

                break;
            }
        }

        if (bullets[i].x < 0 || bullets[i].x > WINDOW_WIDTH ||
            bullets[i].y < 0 || bullets[i].y > WINDOW_HEIGHT) {
            bullets[i].active = false;
        }
    }
}
