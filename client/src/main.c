#include <SDL.h>
#include <SDL_image.h>
#include <SDL_net.h>
#include <SDL_ttf.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#include "bullet.h"
#include "collision.h"
#include "text.h"
#include "wall.h"

#ifdef _WIN32
#include <SDL2/SDL_main.h>
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600
#define MAXTANKS 4
#define SPEED 100
#define SERVER_PORT 12345
#define MAX_PLAYERS 4
volatile int connectedPlayers = 1;

typedef enum {
    CONNECT,  
} ClientCommand;

typedef enum 
{
    STATE_MENU,
    STATE_RUNNING,
    STATE_SELECT_TANK,
    STATE_EXIT
} GameState;

typedef enum {
    DIALOG_RESULT_NONE,
    DIALOG_RESULT_TRY_AGAIN,
    DIALOG_RESULT_CANCEL
} DialogResult;

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
    Tank* tank;
    Bullet bullets[MAX_BULLETS];
    UDPsocket pSocket;
    IPaddress serverAddress;
    UDPpacket *pPacket;
    int playerNumber; 
    int tankColorId;
    int bulletstopper;
    int lastshottime;
    char ipAddress[64];
    Wall* topLeft;
    Wall* topRight;
    Wall* bottomLeft;
    Wall* bottomRight;
    GameState state;
} Game;

void initiate(Game* game);
bool connectToServer(Game* game, const char* ip, bool *timedOut);
void run(Game* game);
void runMainMenu(Game* game);
void enterServerIp(Game* game);
void selectTank(Game* game);
void loadSelectedTankTexture(Game* game);
void close(Game* game);
bool waitForServerResponse(Game *game, Uint32 timeout_ms);
int runAsServerThread(void* data);
DialogResult showErrorDialog(Game* game, const char* title, const char* message);

int main(int argv, char* args[]) {
    Game game;
    initiate(&game);

    while (game.state != STATE_EXIT) {
        switch (game.state) {
            case STATE_MENU:
                runMainMenu(&game);
                break;
            case STATE_RUNNING:
                run(&game);
                break;
            case STATE_SELECT_TANK:
                selectTank(&game);
                break;
            default:
                game.state = STATE_EXIT;
                break;
        }
    }

    close(&game);
    return 0;
}

void initiate(Game *game) 
{
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    game->pWindow = SDL_CreateWindow("Ricochet Tank", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    game->pRenderer = SDL_CreateRenderer(game->pWindow, -1, SDL_RENDERER_ACCELERATED);

    initTank(game->pRenderer);
    loadHeartTexture(game->pRenderer);
    loadBulletTexture(game->pRenderer);
    initTextSystem("../lib/resources/Orbitron-Bold.ttf", 32);

    SDL_Surface *bgSurface = IMG_Load("../lib/resources/background.png");
    game->pBackground = SDL_CreateTextureFromSurface(game->pRenderer, bgSurface);
    SDL_FreeSurface(bgSurface);

    initiate_timer(&game->timer);

    game->tank = createTank();
    setTankPosition(game->tank, 400, 300);
    setTankAngle(game->tank, 0);
    setTankHealth(game->tank, 3);

    game->state = STATE_MENU;
    game->pPacket = NULL;
    game->pSocket = NULL;
    game->tankColorId = 0;
}

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


void enterServerIp(Game* game) {
    SDL_StartTextInput();
    SDL_Texture* background = IMG_LoadTexture(game->pRenderer, "../lib/resources/selTankBg.png");
    
    char inputBuffer[64] = "";
    bool entering = true;

    while (entering) {
        while (SDL_PollEvent(&game->event)) {
            if (game->event.type == SDL_QUIT) {
                game->state = STATE_EXIT;
                entering = false;
            } else if (game->event.type == SDL_TEXTINPUT) {
                if (strlen(inputBuffer) + strlen(game->event.text.text) < 63) {
                    strcat(inputBuffer, game->event.text.text);
                }
            } else if (game->event.type == SDL_KEYDOWN) {
                if (game->event.key.keysym.sym == SDLK_BACKSPACE && strlen(inputBuffer) > 0) {
                    inputBuffer[strlen(inputBuffer) - 1] = '\0';
                } else if (game->event.key.keysym.sym == SDLK_RETURN) {
                    strncpy(game->ipAddress, inputBuffer, sizeof(game->ipAddress));
                    entering = false;
                } else if (game->event.key.keysym.sym == SDLK_ESCAPE) {
                    entering = false;
                }
            }
        }

            SDL_RenderClear(game->pRenderer); // först rensa
            SDL_RenderCopy(game->pRenderer, background, NULL, NULL); // sen rita bilden

            SDL_Color white = {255, 255, 255, 255};
            renderText(game->pRenderer, "Skriv IP och tryck ENTER:", 175, 100, white);
            renderText(game->pRenderer, inputBuffer, 175, 150, white);

            SDL_RenderPresent(game->pRenderer);
            SDL_Delay(16);

    }

    SDL_StopTextInput();
    SDL_DestroyTexture(background);

}


DialogResult showErrorDialog(Game* game, const char* title, const char* message) {
    TTF_Font* font = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 24);
    if (!font) {
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
        return DIALOG_RESULT_CANCEL;
    }

    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* titleSurface = TTF_RenderText_Solid(font, title, textColor);
    SDL_Surface* messageSurface = TTF_RenderText_Solid(font, message, textColor);
    if (!titleSurface || !messageSurface) {
        SDL_Log("TTF_RenderText_Solid failed: %s", TTF_GetError());
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }

    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(game->pRenderer, titleSurface);
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(game->pRenderer, messageSurface);
    SDL_FreeSurface(titleSurface);
    SDL_FreeSurface(messageSurface);
    if (!titleTexture || !messageTexture) {
        SDL_Log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }

    SDL_Surface* tryAgainSurface = TTF_RenderText_Solid(font, "Try Again", textColor);
    SDL_Surface* cancelSurface = TTF_RenderText_Solid(font, "Cancel", textColor);
    if (!tryAgainSurface || !cancelSurface) {
        SDL_Log("TTF_RenderText_Solid failed: %s", TTF_GetError());
        SDL_DestroyTexture(titleTexture);
        SDL_DestroyTexture(messageTexture);
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }

    SDL_Texture* tryAgainTexture = SDL_CreateTextureFromSurface(game->pRenderer, tryAgainSurface);
    SDL_Texture* cancelTexture = SDL_CreateTextureFromSurface(game->pRenderer, cancelSurface);
    SDL_FreeSurface(tryAgainSurface);
    SDL_FreeSurface(cancelSurface);
    if (!tryAgainTexture || !cancelTexture) {
        SDL_Log("SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
        SDL_DestroyTexture(titleTexture);
        SDL_DestroyTexture(messageTexture);
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }

    int dialogW = 400;
    int dialogH = 200;
    SDL_Rect dialogRect = {(WINDOW_WIDTH - dialogW) / 2, (WINDOW_HEIGHT - dialogH) / 2, dialogW, dialogH};

    int titleW, titleH, messageW, messageH;
    SDL_QueryTexture(titleTexture, NULL, NULL, &titleW, &titleH);
    SDL_QueryTexture(messageTexture, NULL, NULL, &messageW, &messageH);
    SDL_Rect titleRect = {dialogRect.x + (dialogW - titleW) / 2, dialogRect.y + 20, titleW, titleH};
    SDL_Rect messageRect = {dialogRect.x + (dialogW - messageW) / 2, dialogRect.y + 50, messageW, messageH};

    int buttonW = 100;
    int buttonH = 40;
    int tryAgainW, tryAgainH, cancelW, cancelH;
    SDL_QueryTexture(tryAgainTexture, NULL, NULL, &tryAgainW, &tryAgainH);
    SDL_QueryTexture(cancelTexture, NULL, NULL, &cancelW, &cancelH);
    SDL_Rect tryAgainButtonRect = {dialogRect.x + 80, dialogRect.y + dialogH - 60, buttonW, buttonH};
    SDL_Rect cancelButtonRect = {dialogRect.x + dialogW - 80 - buttonW, dialogRect.y + dialogH - 60, buttonW, buttonH};
    SDL_Rect tryAgainTextRect = {tryAgainButtonRect.x + (buttonW - tryAgainW) / 2, tryAgainButtonRect.y + (buttonH - tryAgainH) / 2, tryAgainW, tryAgainH};
    SDL_Rect cancelTextRect = {cancelButtonRect.x + (buttonW - cancelW) / 2, cancelButtonRect.y + (buttonH - cancelH) / 2, cancelW, cancelH};

    bool inDialog = true;
    DialogResult result = DIALOG_RESULT_NONE;

    while (inDialog) {
        while (SDL_PollEvent(&game->event)) {
            if (game->event.type == SDL_QUIT) {
                game->state = STATE_EXIT;
                inDialog = false;
                result = DIALOG_RESULT_CANCEL;
            } else if (game->event.type == SDL_MOUSEBUTTONDOWN) {
                int x = game->event.button.x;
                int y = game->event.button.y;
                if (SDL_PointInRect(&(SDL_Point){x, y}, &tryAgainButtonRect)) {
                    result = DIALOG_RESULT_TRY_AGAIN;
                    inDialog = false;
                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &cancelButtonRect)) {
                    result = DIALOG_RESULT_CANCEL;
                    inDialog = false;
                }
            }
        }

        SDL_SetRenderDrawColor(game->pRenderer, 50, 50, 50, 255); // background
        SDL_RenderFillRect(game->pRenderer, &dialogRect);
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255); // White border
        SDL_RenderDrawRect(game->pRenderer, &dialogRect);

        SDL_RenderCopy(game->pRenderer, titleTexture, NULL, &titleRect);
        SDL_RenderCopy(game->pRenderer, messageTexture, NULL, &messageRect);

        SDL_SetRenderDrawColor(game->pRenderer, 100, 100, 100, 255); // Button background
        SDL_RenderFillRect(game->pRenderer, &tryAgainButtonRect);
        SDL_RenderFillRect(game->pRenderer, &cancelButtonRect);
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255); // Button border
        SDL_RenderDrawRect(game->pRenderer, &tryAgainButtonRect);
        SDL_RenderDrawRect(game->pRenderer, &cancelButtonRect);

        SDL_RenderCopy(game->pRenderer, tryAgainTexture, NULL, &tryAgainTextRect);
        SDL_RenderCopy(game->pRenderer, cancelTexture, NULL, &cancelTextRect);

        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(titleTexture);
    SDL_DestroyTexture(messageTexture);
    SDL_DestroyTexture(tryAgainTexture);
    SDL_DestroyTexture(cancelTexture);
    TTF_CloseFont(font);

    return result;
}

void runMainMenu(Game* game) {
    bool inMenu = true;

    SDL_Texture* bg = IMG_LoadTexture(game->pRenderer, "../lib/resources/menu_bg.png");
    SDL_Texture* btnHost = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_host.png");
    SDL_Texture* btnConnect = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_connect.png");
    SDL_Texture* btnSelectTank = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_select_tank.png");
    SDL_Texture* btnExit = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_exit.png");

    SDL_Rect rectHost = {250, 290, 300, 60};
    SDL_Rect rectConnect = {250, 370, 300, 60};
    SDL_Rect rectSelectTank = {250, 450, 300, 60};
    SDL_Rect rectExit = {250, 530, 300, 60};

    while (inMenu) {
        while (SDL_PollEvent(&game->event)) {
            if (game->event.type == SDL_QUIT) {
                game->state = STATE_EXIT;
                inMenu = false;
            } else if (game->event.type == SDL_MOUSEBUTTONDOWN) {
                int x = game->event.button.x;
                int y = game->event.button.y;

                if (SDL_PointInRect(&(SDL_Point){x, y}, &rectHost)) {
                    SDLNet_Init();
                    connectedPlayers = 1; // hosten själv
                    SDL_CreateThread(runAsServerThread, "ServerThread", NULL);

                    SDL_Texture* bgWait = IMG_LoadTexture(game->pRenderer, "../lib/resources/menu_bg.png");
                    bool waiting = true;

                    while (waiting && connectedPlayers < MAX_PLAYERS) {
                        while (SDL_PollEvent(&game->event)) {
                            if (game->event.type == SDL_QUIT) {
                                game->state = STATE_EXIT;
                                inMenu = false;
                                waiting = false;
                                break;
                            } else if (game->event.type == SDL_KEYDOWN &&
                                       game->event.key.keysym.sym == SDLK_ESCAPE) {
                                waiting = false;
                                break;
                            }
                        }

                        SDL_RenderClear(game->pRenderer);
                        SDL_RenderCopy(game->pRenderer, bgWait, NULL, NULL);

                        SDL_Color white = {255, 255, 255, 255};
                        char statusText[64];
                        snprintf(statusText, sizeof(statusText), "Väntar på spelare... %d/%d inne", connectedPlayers, MAX_PLAYERS);
                        renderText(game->pRenderer, statusText, 200, 300, white);
                        renderText(game->pRenderer, "Tryck ESC för att avbryta", 200, 360, white);

                        SDL_RenderPresent(game->pRenderer);
                        SDL_Delay(100);
                    }

                    SDL_DestroyTexture(bgWait);

                    if (connectedPlayers >= MAX_PLAYERS && game->state != STATE_EXIT) {
                        loadSelectedTankTexture(game);
                        game->state = STATE_RUNNING;
                        inMenu = false;
                    } else {
                        connectedPlayers = 0;
                        SDLNet_Quit();
                    }

                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &rectConnect)) {
                    enterServerIp(game);
                    bool tryConnect = true;

                    while (tryConnect) {
                        bool timedOut = false;
                        if (connectToServer(game, game->ipAddress, &timedOut)) {
                            loadSelectedTankTexture(game);
                            game->state = STATE_RUNNING;
                            inMenu = false;
                            tryConnect = false;
                        } else {
                            DialogResult result = showErrorDialog(game, "ERROR", timedOut ? "Kunde inte ansluta till servern." : "Kunde inte ansluta till servern.");
                            if (result == DIALOG_RESULT_TRY_AGAIN) {
                                enterServerIp(game);
                                tryConnect = true;
                            } else {
                                tryConnect = false;
                            }
                        }
                    }

                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &rectSelectTank)) {
                    game->state = STATE_SELECT_TANK;
                    inMenu = false;
                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &rectExit)) {
                    game->state = STATE_EXIT;
                    inMenu = false;
                }
            }
        }

        SDL_SetRenderDrawColor(game->pRenderer, 0, 0, 0, 255);
        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, bg, NULL, NULL);
        SDL_RenderCopy(game->pRenderer, btnHost, NULL, &rectHost);
        SDL_RenderCopy(game->pRenderer, btnConnect, NULL, &rectConnect);
        SDL_RenderCopy(game->pRenderer, btnSelectTank, NULL, &rectSelectTank);
        SDL_RenderCopy(game->pRenderer, btnExit, NULL, &rectExit);
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(bg);
    SDL_DestroyTexture(btnHost);
    SDL_DestroyTexture(btnConnect);
    SDL_DestroyTexture(btnSelectTank);
    SDL_DestroyTexture(btnExit);
}


int runAsServerThread(void* data) {
    UDPsocket serverSocket = SDLNet_UDP_Open(SERVER_PORT);
    if (!serverSocket) {
        SDL_Log("Could not open UDP socket: %s", SDLNet_GetError());
        return -1;
    }

    UDPpacket* packet = SDLNet_AllocPacket(512);
    if (!packet) {
        SDL_Log("Could not allocate packet: %s", SDLNet_GetError());
        SDLNet_UDP_Close(serverSocket);
        return -1;
    }

    int playerNumber = 1;

    while (connectedPlayers < MAX_PLAYERS) {
        if (SDLNet_UDP_Recv(serverSocket, packet)) {
            ClientData request;
            memcpy(&request, packet->data, sizeof(ClientData));

            if (request.command == CONNECT) {
                ClientData response;
                response.command = CONNECT;
                response.playerNumber = playerNumber++;

                memcpy(packet->data, &response, sizeof(ClientData));
                packet->len = sizeof(ClientData);
                SDLNet_UDP_Send(serverSocket, -1, packet);

                SDL_Log("Spelare %d ansluten", response.playerNumber);
                connectedPlayers++;
            }
        }
        SDL_Delay(10);
    }

    SDLNet_FreePacket(packet);
    SDLNet_UDP_Close(serverSocket);
    return 0;
}

void run(Game *game) 
{
    SDL_Rect tankRect = getTankRect(game->tank);
    float shipX = tankRect.x;
    float shipY = tankRect.y;
    float angle = getTankAngle(game->tank);
    float shipVelocityX = 0;
    float shipVelocityY = 0;
    bool closeWindow = false;
    bool up = false, down = false;
    int thickness = 20;
    int length = 80;

    game->topLeft = createWall(100, 100, thickness, length, WALL_TOP_LEFT);
    game->topRight = createWall(WINDOW_WIDTH - 100 - length, 100, thickness, length, WALL_TOP_RIGHT);
    game->bottomLeft = createWall(100, WINDOW_HEIGHT - 100 - length, thickness, length, WALL_BOTTOM_LEFT);
    game->bottomRight = createWall(WINDOW_WIDTH - 100 - length, WINDOW_HEIGHT - 100 - length, thickness, length, WALL_BOTTOM_RIGHT);

    while (!closeWindow) {
        update_timer(&game->timer);
        float dt = get_timer(&game->timer);

        while (SDL_PollEvent(&game->event)) {
            switch (game->event.type) {
                case SDL_QUIT:
                    closeWindow = true;
                    game->state = STATE_EXIT;
                    break;

                case SDL_KEYDOWN:
                    switch (game->event.key.keysym.scancode) {
                        case SDL_SCANCODE_SPACE:
                            game->bulletstopper = 0;
                            if(game->bulletstopper == 0)
                            {
                                if(SDL_GetTicks() - game->lastshottime > 200)
                                {
                                    for(int i = 0;i < MAX_BULLETS;i++)
                                    {
                                        if(!game->bullets[i].active)
                                        {
                                            fireBullet(&game->bullets[i], shipX + tankRect.w / 2, shipY + tankRect.h / 2, angle, 0);
                                            game->lastshottime = SDL_GetTicks();
                                            break;
                                        }
                                    }
                                    game->bulletstopper = 1;
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
                        case SDL_SCANCODE_ESCAPE:
                            game->state = STATE_MENU;
                            closeWindow = true;
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

        SDL_Rect futureTank = getTankRect(game->tank);
        futureTank.x = (int)(shipX + shipVelocityX * dt);
        futureTank.y = (int)(shipY + shipVelocityY * dt);

        if (!wallCheckCollision(game->topLeft, &futureTank) &&
            !wallCheckCollision(game->topRight, &futureTank) &&
            !wallCheckCollision(game->bottomLeft, &futureTank) &&
            !wallCheckCollision(game->bottomRight, &futureTank)) 
        {
            shipX += shipVelocityX * dt;
            shipY += shipVelocityY * dt;
        }

        if (shipX < 0) shipX = 0;
        if (shipY < 0) shipY = 0;
        if (shipX > WINDOW_WIDTH - tankRect.w) shipX = WINDOW_WIDTH - tankRect.w;
        if (shipY > WINDOW_HEIGHT - tankRect.h) shipY = WINDOW_HEIGHT - tankRect.h;

        if (shipX < 0) shipX = 0;
        if (shipY < 0) shipY = 0;
        if (shipX > WINDOW_WIDTH - tankRect.w) shipX = WINDOW_WIDTH - tankRect.w;
        if (shipY > WINDOW_HEIGHT - tankRect.h) shipY = WINDOW_HEIGHT - tankRect.h;

        setTankPosition(game->tank, shipX, shipY);
        setTankAngle(game->tank, angle);

        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);

        renderWall(game->pRenderer, game->topLeft);
        renderWall(game->pRenderer, game->topRight);
        renderWall(game->pRenderer, game->bottomLeft);
        renderWall(game->pRenderer, game->bottomRight);


        if (isTankAlive(game->tank)) {
            drawTank(game->pRenderer, game->tank, game->pTankpicture);
            renderTankHealth(game->pRenderer, 3);  // Du kan byta ut 3 mot en getter vid behov
        }

        for (int i = 0; i < MAX_BULLETS; i++) {
            updateBullet(&game->bullets[i], dt);
            if (game->bullets[i].active) {
                SDL_Rect bulletRect = {
                    (int)game->bullets[i].rect.x,
                    (int)game->bullets[i].rect.y,
                    (int)game->bullets[i].rect.w,
                    (int)game->bullets[i].rect.h
                };

                if (wallCheckCollision(game->topLeft, &bulletRect) ||
                    wallCheckCollision(game->bottomLeft, &bulletRect) ||
                    wallCheckCollision(game->topRight, &bulletRect) ||
                    wallCheckCollision(game->bottomRight, &bulletRect)) {

                    // Invertera riktning
                    if (wallHitsVertical(game->topLeft, game->topRight, game->bottomLeft, game->bottomRight, &bulletRect)) {
                        game->bullets[i].velocityX *= -1;
                    }
                    if (wallHitsHorizontal(game->topLeft, game->topRight, game->bottomLeft, game->bottomRight, &bulletRect)) {
                        game->bullets[i].velocityY *= -1;
                    }
                }
            }

            SDL_Rect tankRect = getTankRect(game->tank);
            SDL_FRect bulletRect = game->bullets[i].rect;

            if (game->bullets[i].active &&
                game->bullets[i].ownerId != 0 &&
                checkCollision(&tankRect, &bulletRect)) {
                game->bullets[i].active = false;
            }
            renderBullet(game->pRenderer, &game->bullets[i]);
        }

        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(1000 / 60);
    }
}

void selectTank(Game* game) {
    bool selecting = true;
    int currentSelection = 0;

    SDL_Texture* background = IMG_LoadTexture(game->pRenderer, "../lib/resources/selTankBg.png");

    SDL_Texture* tanks[MAXTANKS];
    const char* tankNames[MAXTANKS] = {"Ironclad", "Blockbuster", "Ghost Walker", "Shadow Reaper"};
    tanks[0] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank.png");
    tanks[1] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_lego.png");
    tanks[2] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_light.png");
    tanks[3] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_dark.png");

    SDL_Rect tankRect = {250, 150, 300, 400};

    float angle = 0.0f;
    bool swingRight = true;
    float swingSpeed = 30.0f;
    float maxSwingAngle = 5.0f;

    initiate_timer(&game->timer);

    while (selecting) {
        update_timer(&game->timer);
        float dt = get_timer(&game->timer);

        while (SDL_PollEvent(&game->event)) {
            if (game->event.type == SDL_QUIT) {
                game->state = STATE_EXIT;
                selecting = false;
            } else if (game->event.type == SDL_KEYDOWN) {
                switch (game->event.key.keysym.sym) {
                    case SDLK_LEFT:
                        currentSelection = (currentSelection - 1 + MAXTANKS) % MAXTANKS;
                        break;
                    case SDLK_RIGHT:
                        currentSelection = (currentSelection + 1) % MAXTANKS;
                        break;
                    case SDLK_RETURN:
                        game->tankColorId = currentSelection;
                        game->state = STATE_MENU;
                        selecting = false;
                        break;
                    case SDLK_ESCAPE:
                        game->state = STATE_MENU;
                        selecting = false;
                        break;
                    default:
                        break;
                }
            }
        }

        if (swingRight) {
            angle += swingSpeed * dt;
            if (angle >= maxSwingAngle) {
                angle = maxSwingAngle;
                swingRight = false;
            }
        } else {
            angle -= swingSpeed * dt;
            if (angle <= -maxSwingAngle) {
                angle = -maxSwingAngle;
                swingRight = true;
            }
        }

        SDL_RenderCopy(game->pRenderer, background, NULL, NULL);

        SDL_Color white = {255, 255, 255, 255};
        renderText(game->pRenderer, tankNames[currentSelection], (WINDOW_WIDTH / 2) - 100, 50, white);

        SDL_Color gray = {180, 180, 180, 255};
        renderText(game->pRenderer, "Press ENTER to choose", (WINDOW_WIDTH / 2) - 200, 100, gray);

        SDL_RenderCopyEx(game->pRenderer, tanks[currentSelection], NULL, &tankRect, angle, NULL, SDL_FLIP_NONE);

        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }

    for (int i = 0; i < MAXTANKS; i++) {
        SDL_DestroyTexture(tanks[i]);
    }
    SDL_DestroyTexture(background);
}

void loadSelectedTankTexture(Game* game) {
    if (game->pTankpicture != NULL) {
        SDL_DestroyTexture(game->pTankpicture);
        game->pTankpicture = NULL;
    }

    switch (game->tankColorId) {
        case 0:
            game->pTankpicture = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank.png");
            break;
        case 1:
            game->pTankpicture = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_lego.png");
            break;
        case 2:
            game->pTankpicture = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_light.png");
            break;
        case 3:
            game->pTankpicture = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_dark.png");
            break;
        default:
            game->pTankpicture = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank.png");
            break;
    }
}

bool waitForServerResponse(Game *game, Uint32 timeout_ms) {
    SDLNet_SocketSet socketSet = SDLNet_AllocSocketSet(1);
    if (!socketSet) {
        printf("SDLNet_AllocSocketSet: %s\n", SDLNet_GetError());
        return false;
    }

    SDLNet_UDP_AddSocket(socketSet, game->pSocket);

    int numready = SDLNet_CheckSockets(socketSet, timeout_ms);

    SDLNet_FreeSocketSet(socketSet);

    if (numready == -1) {
        printf("SDLNet_CheckSockets error: %s\n", SDLNet_GetError());
        return false;
    }

    return numready > 0;
}
void close(Game *game) 
{
    destroyTankInstance(game->tank);
    destroyTank();
    destroyBulletTexture();
    destroyHeartTexture();
    destroyWall(game->topLeft);
    destroyWall(game->topRight);
    destroyWall(game->bottomLeft);
    destroyWall(game->bottomRight);
    SDL_DestroyTexture(game->pTankpicture);
    SDL_DestroyTexture(game->pBackground);
    SDL_DestroyRenderer(game->pRenderer);
    SDL_DestroyWindow(game->pWindow);
    if (game->pPacket != NULL) {
        SDLNet_FreePacket(game->pPacket);
        game->pPacket = NULL;
    }
    if (game->pSocket != NULL) {
        SDLNet_UDP_Close(game->pSocket);
        game->pSocket = NULL;
    }
    closeTextSystem();
    SDLNet_Quit();
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
}