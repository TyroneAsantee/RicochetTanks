#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_net.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#include "bullet.h"
#include "collision.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define SPEED 100
#define SERVER_PORT 12345

typedef enum {
    CONNECT,  
} ClientCommand;

typedef struct {
    ClientCommand command;
    int playerNumber; 
} ClientData;


typedef struct {
    SDL_Window *pWindow;
    SDL_Renderer *pRenderer;
    SDL_Texture *pBackground;
    SDL_Texture *pTankpicture;
    SDL_Event event;
    float angle;
    Timer timer;
    Tank tank;
    Bullet bullets[MAX_BULLETS];
    UDPsocket pSocket;
    IPaddress serverAddress;
    UDPpacket *pPacket;
    int playerNumber; 

} Game;

void initiate(Game* game);
bool checkCollision(SDL_Rect* a, SDL_FRect* b);
void run(Game* game);
void close(Game* game);

int main(int argv, char** args) {
    Game game;
    initiate(&game);
    run(&game);
    close(&game);
    return 0;
}

void initiate(Game *game) 
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL Init Error: %s", SDL_GetError());
        return;
    }

    if (!(IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG)) {
        SDL_Log("SDL_image Init Error: %s", SDL_GetError());
        SDL_Quit();
        return;
    }

    if (SDLNet_Init() != 0) {
        SDL_Log("SDLNet Init Error: %s", SDLNet_GetError());
        IMG_Quit();
        SDL_Quit();
        return;
    }

    game->pWindow = SDL_CreateWindow("Ricochet Tank", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    if (!game->pWindow) {
        SDL_Log("Window error: %s", SDL_GetError());
        return;
    }

    game->pRenderer = SDL_CreateRenderer(game->pWindow, -1, SDL_RENDERER_ACCELERATED);
    if (!game->pRenderer) {
        SDL_Log("Renderer error: %s", SDL_GetError());
        SDL_DestroyWindow(game->pWindow);
        SDL_Quit();
        return;
    }
    game->pSocket = SDLNet_UDP_Open(0);  // 0 lets the system assign a port
    if (!game->pSocket) {
        SDL_Log("UDP Socket Open Error: %s", SDLNet_GetError());
        SDL_DestroyRenderer(game->pRenderer);
        SDL_DestroyWindow(game->pWindow);
        SDLNet_Quit();
        IMG_Quit();
        SDL_Quit();
        return;
    }

    if (SDLNet_ResolveHost(&game->serverAddress, "127.0.0.1", SERVER_PORT) != 0) {
        SDL_Log("Resolve Host Error: %s", SDLNet_GetError());
        SDLNet_UDP_Close(game->pSocket);
        SDL_DestroyRenderer(game->pRenderer);
        SDL_DestroyWindow(game->pWindow);
        SDLNet_Quit();
        IMG_Quit();
        SDL_Quit();
        return;
    }

    game->pPacket = SDLNet_AllocPacket(512);
    if (!game->pPacket) {
        SDL_Log("Packet Alloc Error: %s", SDLNet_GetError());
        SDLNet_UDP_Close(game->pSocket);
        SDL_DestroyRenderer(game->pRenderer);
        SDL_DestroyWindow(game->pWindow);
        SDLNet_Quit();
        IMG_Quit();
        SDL_Quit();
        return;
    }

    ClientData connectData = {CONNECT, 0};
    memcpy(game->pPacket->data, &connectData, sizeof(ClientData));
    game->pPacket->len = sizeof(ClientData);
    game->pPacket->address = game->serverAddress;
    if (SDLNet_UDP_Send(game->pSocket, -1, game->pPacket) == 0) {
        SDL_Log("UDP Send Error: %s", SDLNet_GetError());
        SDLNet_FreePacket(game->pPacket);
        SDLNet_UDP_Close(game->pSocket);
        SDL_DestroyRenderer(game->pRenderer);
        SDL_DestroyWindow(game->pWindow);
        SDLNet_Quit();
        IMG_Quit();
        SDL_Quit();
        return;
    }

    bool received = false;
    Uint32 timeout = SDL_GetTicks() + 5000;  // 5-second timeout
    while (!received && SDL_GetTicks() < timeout) {
        if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
            ClientData response;
            memcpy(&response, game->pPacket->data, sizeof(ClientData));
            if (response.command == CONNECT) {
                game->playerNumber = response.playerNumber;
                SDL_Log("Connected to server, assigned player number: %d", game->playerNumber);
                received = true;
            }
        }
        SDL_Delay(10);
    }

    if (!received) {
        SDL_Log("Failed to connect to server: No response received");
        SDLNet_FreePacket(game->pPacket);
        SDLNet_UDP_Close(game->pSocket);
        SDL_DestroyRenderer(game->pRenderer);
        SDL_DestroyWindow(game->pWindow);
        SDLNet_Quit();
        IMG_Quit();
        SDL_Quit();
        return;
    }


    initTank(game->pRenderer);
    loadHeartTexture(game->pRenderer);
    loadBulletTexture(game->pRenderer);

    game->tank.rect.w = 40;
    game->tank.rect.h = 40;
    game->tank.rect.x = (WINDOW_WIDTH - game->tank.rect.w) / 2;
    game->tank.rect.y = (WINDOW_HEIGHT - game->tank.rect.h) / 2;
    game->tank.angle = 0.0f;
    game->tank.velocityX = 0;
    game->tank.velocityY = 0;
    game->tank.health = 3;
    game->tank.alive = true;

    for (int i = 0; i < MAX_BULLETS; i++) {
        initBullet(&game->bullets[i]);
    }

    SDL_Surface *pTankSurface = IMG_Load("resources/tank.png");
    game->pTankpicture = SDL_CreateTextureFromSurface(game->pRenderer, pTankSurface);
    SDL_FreeSurface(pTankSurface);

    SDL_Surface *bgSurface = IMG_Load("resources/background.png");
    if (!bgSurface) {
        SDL_Log("Could not load background: %s", IMG_GetError());
        SDLNet_FreePacket(game->pPacket);
        SDLNet_UDP_Close(game->pSocket);
        SDL_DestroyRenderer(game->pRenderer);
        SDL_DestroyWindow(game->pWindow);
        SDLNet_Quit();
        IMG_Quit();
        SDL_Quit();
        return;
    }
    game->pBackground = SDL_CreateTextureFromSurface(game->pRenderer, bgSurface);
    SDL_FreeSurface(bgSurface);

    initiate_timer(&game->timer);
}

void run(Game *game) 
{
    float shipX = game->tank.rect.x;
    float shipY = game->tank.rect.y;
    float angle = game->tank.angle;
    float shipVelocityX = 0;
    float shipVelocityY = 0;
    bool closeWindow = false;
    bool up = false, down = false;

    while (!closeWindow) {
        update_timer(&game->timer);
        float dt = get_timer(&game->timer);

        while (SDL_PollEvent(&game->event)) {
            switch (game->event.type) {
                case SDL_QUIT:
                    closeWindow = true;
                    break;

                case SDL_KEYDOWN:
                    switch (game->event.key.keysym.scancode) {
                        case SDL_SCANCODE_SPACE:
                            for (int i = 0; i < MAX_BULLETS; i++) {
                                if (!game->bullets[i].active) {
                                    fireBullet(&game->bullets[i],
                                        shipX + game->tank.rect.w / 2,
                                        shipY + game->tank.rect.h / 2,
                                        angle,
                                        0);
                                    break;
                                }
                            }
                            break;
                        case SDL_SCANCODE_W:
                        case SDL_SCANCODE_UP:
                            up = true;
                            break;
                        case SDL_SCANCODE_S:
                        case SDL_SCANCODE_DOWN:
                            down = true;
                            break;
                        case SDL_SCANCODE_A:
                        case SDL_SCANCODE_LEFT:
                            angle -= 10.0f;
                            break;
                        case SDL_SCANCODE_D:
                        case SDL_SCANCODE_RIGHT:
                            angle += 10.0f;
                            break;
                        default:
                            break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (game->event.key.keysym.scancode) {
                        case SDL_SCANCODE_W:
                        case SDL_SCANCODE_UP:
                            up = false;
                            break;
                        case SDL_SCANCODE_S:
                        case SDL_SCANCODE_DOWN:
                            down = false;
                            break;
                        default:
                            break;
                    }
                    break;
            }
        }

        shipVelocityX = 0;
        shipVelocityY = 0;

        if (up && !down) {
            float radians = (angle - 90.0f) * M_PI / 180.0f;
            shipVelocityX = cos(radians) * SPEED;
            shipVelocityY = sin(radians) * SPEED;
        }
        if (down && !up) {
            float radians = (angle - 90.0f) * M_PI / 180.0f;
            shipVelocityX = -cos(radians) * SPEED;
            shipVelocityY = -sin(radians) * SPEED;
        }

        shipX += shipVelocityX * dt;
        shipY += shipVelocityY * dt;

        if (shipX < 0) shipX = 0;
        if (shipY < 0) shipY = 0;
        if (shipX > WINDOW_WIDTH - game->tank.rect.w) shipX = WINDOW_WIDTH - game->tank.rect.w;
        if (shipY > WINDOW_HEIGHT - game->tank.rect.h) shipY = WINDOW_HEIGHT - game->tank.rect.h;

        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);
        
        if (game->tank.alive) {
            game->tank.rect.x = shipX;
            game->tank.rect.y = shipY;
            game->tank.angle = angle;
        
            SDL_RenderCopyEx(game->pRenderer, game->pTankpicture, NULL, &game->tank.rect, game->tank.angle, NULL, SDL_FLIP_NONE);
            renderTankHealth(game->pRenderer, game->tank.health);
        }        

        for (int i = 0; i < MAX_BULLETS; i++) {
            updateBullet(&game->bullets[i], dt);
        
            if (game->bullets[i].active &&
                game->bullets[i].ownerId != 0 &&
                checkCollision(&game->tank.rect, &game->bullets[i].rect)) {
                game->bullets[i].active = false;
        
                if (game->tank.alive && game->tank.health > 0) {
                    game->tank.health--;
                    SDL_Log("Tank is hit! health: %d", game->tank.health);
        
                    if (game->tank.health <= 0) {
                        game->tank.alive = false;
                        SDL_Log("Tank is dead!");
                    }
                }
            }
        
            renderBullet(game->pRenderer, &game->bullets[i]);
        }        

        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(1000 / 60);
    }
}

void close(Game *game) 
{
    destroyTank();
    destroyBulletTexture();
    destroyHeartTexture();
    SDL_DestroyTexture(game->pTankpicture);
    SDL_DestroyTexture(game->pBackground);
    SDL_DestroyRenderer(game->pRenderer);
    SDL_DestroyWindow(game->pWindow);
    SDLNet_FreePacket(game->pPacket);
    SDLNet_UDP_Close(game->pSocket);
    SDLNet_Quit();
    IMG_Quit();
    SDL_Quit();
}
