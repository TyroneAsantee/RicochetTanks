#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_net.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#include "bullet.h"
#include "collision.h"
#include "text.h"

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

typedef enum 
{
    STATE_MENU,
    STATE_RUNNING,
    STATE_SELECT_TANK,
    STATE_EXIT
} GameState;

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
    GameState state;
} Game;

void initiate(Game* game);
bool connectToServer(Game* game);
void run(Game* game);
void runMainMenu(Game* game);
void selectTank(Game* game);
void loadSelectedTankTexture(Game* game);
void close(Game* game);

int main(int argv, char** args) {
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

bool connectToServer(Game* game) {
    if (SDLNet_Init() != 0) return false;
    game->pSocket = SDLNet_UDP_Open(0);
    if (!game->pSocket) return false;
    if (SDLNet_ResolveHost(&game->serverAddress, "127.0.0.1", SERVER_PORT) != 0) return false;
    game->pPacket = SDLNet_AllocPacket(512);
    if (!game->pPacket) return false;

    ClientData connectData = {CONNECT, 0};
    memcpy(game->pPacket->data, &connectData, sizeof(ClientData));
    game->pPacket->len = sizeof(ClientData);
    game->pPacket->address = game->serverAddress;

    if (SDLNet_UDP_Send(game->pSocket, -1, game->pPacket) == 0) return false;

    Uint32 timeout = SDL_GetTicks() + 5000;
    while (SDL_GetTicks() < timeout) {
        if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
            ClientData response;
            memcpy(&response, game->pPacket->data, sizeof(ClientData));
            if (response.command == CONNECT) {
                game->playerNumber = response.playerNumber;
                SDL_Log("Connected as player %d", game->playerNumber);
                return true;
            }
        }
        SDL_Delay(10);
    }

    SDL_Log("Server connection timeout");
    return false;
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
                    loadSelectedTankTexture(game);
                    game->state = STATE_RUNNING;
                    inMenu = false;
                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &rectConnect)) {
                    if (connectToServer(game)) {
                        loadSelectedTankTexture(game);
                        game->state = STATE_RUNNING;
                        inMenu = false;
                    } else {
                        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, "Nätverksfel", "Kunde inte ansluta till server", game->pWindow);
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
                            for (int i = 0; i < MAX_BULLETS; i++) {
                                if (!game->bullets[i].active) {
                                    fireBullet(&game->bullets[i],
                                        shipX + tankRect.w / 2,
                                        shipY + tankRect.h / 2,
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

        shipX += shipVelocityX * dt;
        shipY += shipVelocityY * dt;
        if (shipX < 0) shipX = 0;
        if (shipY < 0) shipY = 0;
        if (shipX > WINDOW_WIDTH - tankRect.w) shipX = WINDOW_WIDTH - tankRect.w;
        if (shipY > WINDOW_HEIGHT - tankRect.h) shipY = WINDOW_HEIGHT - tankRect.h;

        setTankPosition(game->tank, shipX, shipY);
        setTankAngle(game->tank, angle);

        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);

        if (isTankAlive(game->tank)) {
            drawTank(game->pRenderer, game->tank, game->pTankpicture);
            renderTankHealth(game->pRenderer, 3);  // Du kan byta ut 3 mot en getter vid behov
        }

        for (int i = 0; i < MAX_BULLETS; i++) {
            updateBullet(&game->bullets[i], dt);

            SDL_Rect tankRect = getTankRect(game->tank);

            if (game->bullets[i].active &&
                game->bullets[i].ownerId != 0 &&
                checkCollision(&tankRect, &game->bullets[i].rect)) {
                game->bullets[i].active = false;

                // Här kan du lägga till health-- med setTankHealth/getTankHealth
                // men eftersom du inte har en getter än, hoppar vi det tills vidare
            }
            renderBullet(game->pRenderer, &game->bullets[i]);
        }

        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(1000 / 60);
    }
}

void selectTank(Game* game) 
{
    bool selecting = true;
    int currentSelection = 0;
    const int maxTanks = 4;

    SDL_Texture* background = IMG_LoadTexture(game->pRenderer, "../lib/resources/selTankBg.png");


    SDL_Texture* tanks[maxTanks];
    const char* tankNames[maxTanks] = {
                                        "Ironclad",
                                        "Blockbuster",
                                        "Ghost Walker",
                                        "Shadow Reaper"
                                        };
    tanks[0] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank.png");
    tanks[1] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_lego.png");
    tanks[2] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_light.png");
    tanks[3] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_dark.png");

    SDL_Rect tankRect = {250, 150, 300, 400}; // Centralt och lagom stort

    float angle = 0.0f;
    bool swingRight = true;
    float swingSpeed = 30.0f;  // grader per sekund
    float maxSwingAngle = 5.0f; // max +- svängning

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
                        currentSelection = (currentSelection - 1 + maxTanks) % maxTanks;
                        break;
                    case SDLK_RIGHT:
                        currentSelection = (currentSelection + 1) % maxTanks;
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

        //ritar namn
        SDL_Color white = {255, 255, 255, 255};
        renderText(game->pRenderer, tankNames[currentSelection], (WINDOW_WIDTH / 2) - 100, 50, white);

        SDL_Color gray = {180, 180, 180, 255};
        renderText(game->pRenderer, "Press ENTER to choose", (WINDOW_WIDTH / 2) - 200, 100, gray);
        
        //ritar tank
        SDL_RenderCopyEx(game->pRenderer, tanks[currentSelection], NULL, &tankRect, angle, NULL, SDL_FLIP_NONE);
        
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }

    for (int i = 0; i < maxTanks; i++) {
        SDL_DestroyTexture(tanks[i]);
    }
    SDL_DestroyTexture(background);
}

void loadSelectedTankTexture(Game* game)
{
    if (game->pTankpicture != NULL) {
        SDL_DestroyTexture(game->pTankpicture);
        game->pTankpicture = NULL;
    }

    switch (game->tankColorId) 
    {
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

void close(Game *game) 
{
    destroyTankInstance(game->tank);
    destroyTank();
    destroyBulletTexture();
    destroyHeartTexture();
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
    SDL_Quit();
}