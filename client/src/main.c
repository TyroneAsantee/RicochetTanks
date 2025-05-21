#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_net.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#include "bullet.h"
#include "collision.h"
#include "text.h"
#include "wall.h"
#include "network_protocol.h"
#include "tank_server.h"
#include "bullet_server.h"

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
#define MAX_BULLETS_PER_PLAYER 5
volatile int connectedPlayers = 1;

typedef enum {
   DIALOG_RESULT_NONE,
   DIALOG_RESULT_TRY_AGAIN,
   DIALOG_RESULT_CANCEL
} DialogResult;

typedef enum {
    STATE_MENU,
    STATE_SINGLE_PLAYER,
    STATE_RUNNING,
    STATE_SELECT_TANK,
    STATE_EXIT
} GameState;

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
    TankState otherTanks[MAX_PLAYERS];
    SDL_Texture* tankTextures[MAXTANKS];
    int numOtherTanks;
    bool matchOver;
    int winningPlayerID; 
} Game;


void initiate(Game* game);
bool connectToServer(Game* game, const char* ip, bool *timedOut);
void run(Game* game);
void runMainMenu(Game* game);
void enterServerIp(Game* game);
void selectTank(Game* game);
void loadSelectedTankTexture(Game* game);
void runSinglePlayer(Game *game);
void closeGame(Game* game);
void showYouDiedDialog(Game* game);
bool connectToServer(Game* game, const char* ip, bool *timedOut);
void receiveGameState(Game* game);
void sendClientUpdate(Game* game);
DialogResult showErrorDialog(Game* game, const char* title, const char* message);
void showWinnerDialog(Game* game, int winnerID);


int main(int argv, char* args[]) {
   Game game;
   initiate(&game);

   while (game.state != STATE_EXIT) {
       switch (game.state) {
        case STATE_MENU:
               runMainMenu(&game);
               break;
        case STATE_SINGLE_PLAYER: 
               runSinglePlayer(&game);
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
   closeGame(&game);
   return 0;
}


void initiate(Game *game){

    memset(game, 0, sizeof(Game));
    SDL_Init(SDL_INIT_VIDEO);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();
    if (SDLNet_Init() == -1) {
            SDL_Log("SDLNet_Init failed: %s", SDLNet_GetError());
            game->state = STATE_EXIT;
        }
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
    game->state = STATE_MENU;
    game->pPacket = NULL;
    game->pSocket = NULL;
    game->tankColorId = 0;
    game->tank = NULL;
    game->topLeft = NULL;
    game->topRight = NULL;
    game->bottomLeft = NULL;
    game->bottomRight = NULL;
    game->matchOver = false; 
    game->winningPlayerID = -1; 
}


void enterServerIp(Game* game) {
    SDL_StartTextInput();
    SDL_Texture* background = IMG_LoadTexture(game->pRenderer, "../lib/resources/selTankBg.png");
    if (!background) {
        SDL_Log("Failed to load background texture: %s", SDL_GetError());
        SDL_StopTextInput();
        return;
    }
    char inputBuffer[64] = "";
    bool entering = true;
    int inputRectW = 450; 
    int inputRectH = 40;
    int textY = 100;
    int spacing = 50; 
    SDL_Rect inputRect = {175, textY + spacing + 24, inputRectW, inputRectH}; 
    TTF_Font* font = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 24);
    if (!font) {
        SDL_Log("Failed to load font: %s", TTF_GetError());
        SDL_StopTextInput();
        SDL_DestroyTexture(background);
        return;
    }
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
        SDL_RenderClear(game->pRenderer); 
        SDL_RenderCopy(game->pRenderer, background, NULL, NULL); 
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255); 
        SDL_RenderDrawRect(game->pRenderer, &inputRect); 
        SDL_Color white = {255, 255, 255, 255};
        renderText(game->pRenderer, "Type IP And Press ENTER:", 175, 100, white);
        if (strlen(inputBuffer) > 0) {
            int textWidth, textHeight;
            if (TTF_SizeText(font, inputBuffer, &textWidth, &textHeight) == 0) {
                int textX = inputRect.x + 10;  
                int textYCentered = inputRect.y + (inputRectH - textHeight) / 2; 
                renderText(game->pRenderer, inputBuffer, textX, textYCentered, white);
            } else {
                SDL_Log("Failed to calculate text size: %s", TTF_GetError());
                renderText(game->pRenderer, inputBuffer, inputRect.x + 5, inputRect.y + (inputRectH - 24) / 2, white);
            }
        }
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }
    TTF_CloseFont(font);
    SDL_StopTextInput();
    SDL_DestroyTexture(background);
}


void runMainMenu(Game* game) {
    bool inMenu = true;

    SDL_Texture* bg = IMG_LoadTexture(game->pRenderer, "../lib/resources/menu_bg.png");
    SDL_Texture* btnSingle = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_practice.png");
    SDL_Texture* btnConnect = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_connect.png");
    SDL_Texture* btnSelectTank = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_select_tank.png");
    SDL_Texture* btnExit = IMG_LoadTexture(game->pRenderer, "../lib/resources/btn_exit.png");
    SDL_Rect rectSingle = {250, 290, 300, 60};
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
                if (SDL_PointInRect(&(SDL_Point){x, y}, &rectSingle)) {
                    SDL_Delay(200);
                    loadSelectedTankTexture(game);
                    game->state = STATE_SINGLE_PLAYER;
                    inMenu = false;
                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &rectConnect)) {
                    enterServerIp(game);
                    bool tryConnect = true;
                    while (tryConnect) {
                        bool timedOut = false;
                        if (connectToServer(game, game->ipAddress, &timedOut)) {
                            loadSelectedTankTexture(game);
                            game->state = STATE_RUNNING;
                            Uint32 waitStart = SDL_GetTicks();
                            while (!game->tank && SDL_GetTicks() - waitStart < 1000) {
                                receiveGameState(game);
                                SDL_Delay(10);
                            }
                            inMenu = false;
                            return;
                        } else {
                            SDL_Log("ERROR: connectToServer() misslyckades. timedOut=%d", timedOut);
                            DialogResult result = showErrorDialog(game, "ERROR", timedOut ? "Could not connect to server." : "Connection failed.");
                            if (result == DIALOG_RESULT_TRY_AGAIN) {
                                enterServerIp(game);
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
        SDL_RenderCopy(game->pRenderer, btnSingle, NULL, &rectSingle);
        SDL_RenderCopy(game->pRenderer, btnConnect, NULL, &rectConnect);
        SDL_RenderCopy(game->pRenderer, btnSelectTank, NULL, &rectSelectTank);
        SDL_RenderCopy(game->pRenderer, btnExit, NULL, &rectExit);
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }
    SDL_DestroyTexture(bg);
    SDL_DestroyTexture(btnSingle);
    SDL_DestroyTexture(btnConnect);
    SDL_DestroyTexture(btnSelectTank);
    SDL_DestroyTexture(btnExit);
}


void runSinglePlayer(Game *game) {
    game->tank = createTank();
    setTankPosition(game->tank, 400, 300);  
    setTankAngle(game->tank, 0);
    setTankColorId(game->tank, game->tankColorId);
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
                                if(SDL_GetTicks() - game->lastshottime > 700)
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
            renderTankHealth(game->pRenderer, 3);  
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


void run(Game *game) {
    Uint32 start = SDL_GetTicks();
    while (!game->tank && SDL_GetTicks() - start < 3000) {
        receiveGameState(game);
        SDL_Delay(10);
    }
    if (!game->tank) {
        SDL_Log("ERROR: Timeout, game->tank är fortfarande NULL. Avbryter spelstart.");
        game->state = STATE_MENU;
        return;
    }
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
        receiveGameState(game);
        float dt = get_timer(&game->timer);
        if (game->matchOver) { 
            showWinnerDialog(game, game->winningPlayerID);
            game->state = STATE_MENU;
            closeWindow = true;
            break;
        }
        while (SDL_PollEvent(&game->event)) {
            switch (game->event.type) {
                case SDL_QUIT:
                    closeWindow = true;
                    game->state = STATE_EXIT;
                    break;
                case SDL_KEYDOWN:
                    switch (game->event.key.keysym.scancode) {
                        case SDL_SCANCODE_W: up = true; break;
                        case SDL_SCANCODE_S: down = true; break;
                        case SDL_SCANCODE_ESCAPE:
                            closeWindow = true;
                            game->state = STATE_MENU;
                            break;
                        default: break;
                    }
                    break;
                case SDL_KEYUP:
                    switch (game->event.key.keysym.scancode) {
                        case SDL_SCANCODE_W: up = false; break;
                        case SDL_SCANCODE_S: down = false; break;
                        default: break;
                    }
                    break;
            }
        }
        if (game->tank) {
            if (getTankHealth(game->tank) <= 0) {
                destroyTank();
                game->tank = NULL;
                showYouDiedDialog(game);
                game->state = STATE_MENU;
                closeWindow = true;
                break;
            }
            sendClientUpdate(game);
        }
        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);
        renderWall(game->pRenderer, game->topLeft);
        renderWall(game->pRenderer, game->topRight);
        renderWall(game->pRenderer, game->bottomLeft);
        renderWall(game->pRenderer, game->bottomRight);
        for (int i = 0; i < game->numOtherTanks; i++) {
            TankState *tank = &game->otherTanks[i];
            SDL_Rect rect = { tank->x, tank->y, 64, 64 };
            SDL_RenderCopyEx(
                game->pRenderer,
                game->tankTextures[tank->tankColorId],
                NULL, &rect,
                tank->angle,
                NULL,
                SDL_FLIP_NONE
            );
        }
        if (game->tank) {
            SDL_Rect rect = getTankRect(game->tank);
            SDL_RenderCopyEx(
                game->pRenderer,
                game->tankTextures[getTankColorId(game->tank)],
                NULL, &rect,
                getTankAngle(game->tank),
                NULL,
                SDL_FLIP_NONE
            );
            renderTankHealth(game->pRenderer, getTankHealth(game->tank));
        }
        for (int i = 0; i < MAX_PLAYERS * MAX_BULLETS_PER_PLAYER; i++) {
            if (!game->bullets[i].active)
                continue;
            SDL_Rect tankRect = getTankRect(game->tank);
            SDL_FRect bulletRect = game->bullets[i].rect;
            SDL_Rect wallRect = {
                (int)bulletRect.x,
                (int)bulletRect.y,
                (int)bulletRect.w,
                (int)bulletRect.h
            };
            if (wallCheckCollision(game->topLeft, &wallRect) ||
                wallCheckCollision(game->topRight, &wallRect) ||
                wallCheckCollision(game->bottomLeft, &wallRect) ||
                wallCheckCollision(game->bottomRight, &wallRect)) {

                if (wallHitsVertical(game->topLeft, game->topRight, game->bottomLeft, game->bottomRight, &wallRect)) {
                    game->bullets[i].velocityX *= -1;
                }
                if (wallHitsHorizontal(game->topLeft, game->topRight, game->bottomLeft, game->bottomRight, &wallRect)) {
                    game->bullets[i].velocityY *= -1;
                }
            }
            if (game->bullets[i].ownerId != game->playerNumber && checkCollision(&tankRect, &bulletRect)) {
                game->bullets[i].active = false;
            }

            renderBullet(game->pRenderer, &game->bullets[i]);
        }
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
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
    game->tankTextures[0] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank.png");
    game->tankTextures[1] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_lego.png");
    game->tankTextures[2] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_light.png");
    game->tankTextures[3] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_dark.png");
    int id = game->tankColorId;
    if (id < 0 || id >= MAXTANKS) id = 0;
    game->pTankpicture = game->tankTextures[id];
}


DialogResult showErrorDialog(Game* game, const char* title, const char* message) {
    if (!title || strlen(title) == 0 || !message || strlen(message) == 0) {
        SDL_Log("ErrorDialog: Title or message is empty");
        return DIALOG_RESULT_CANCEL;
    }
    TTF_Font* font = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 24);
    if (!font) {
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
        return DIALOG_RESULT_CANCEL;
    }
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* titleSurface = TTF_RenderText_Solid(font, title, textColor);
    if (!titleSurface) {
        SDL_Log("Failed to render title text: %s", TTF_GetError());
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }
    SDL_Surface* messageSurface = TTF_RenderText_Solid(font, message, textColor);
    if (!messageSurface) {
        SDL_Log("Failed to render message text: %s", TTF_GetError());
        SDL_FreeSurface(titleSurface);
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }
    SDL_Surface* tryAgainSurface = TTF_RenderText_Solid(font, "Try Again", textColor);
    if (!tryAgainSurface) {
        SDL_Log("Failed to render try again text: %s", TTF_GetError());
        SDL_FreeSurface(titleSurface);
        SDL_FreeSurface(messageSurface);
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }
    SDL_Surface* cancelSurface = TTF_RenderText_Solid(font, "Cancel", textColor);
    if (!cancelSurface) {
        SDL_Log("Failed to render cancel text: %s", TTF_GetError());
        SDL_FreeSurface(titleSurface);
        SDL_FreeSurface(messageSurface);
        SDL_FreeSurface(tryAgainSurface);
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }
    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(game->pRenderer, titleSurface);
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(game->pRenderer, messageSurface);
    SDL_Texture* tryAgainTexture = SDL_CreateTextureFromSurface(game->pRenderer, tryAgainSurface);
    SDL_Texture* cancelTexture = SDL_CreateTextureFromSurface(game->pRenderer, cancelSurface);
    SDL_FreeSurface(titleSurface);
    SDL_FreeSurface(messageSurface);
    SDL_FreeSurface(tryAgainSurface);
    SDL_FreeSurface(cancelSurface);
    if (!titleTexture || !messageTexture || !tryAgainTexture || !cancelTexture) {
        SDL_Log("Failed to create textures from surfaces: %s", SDL_GetError());
        if (titleTexture) SDL_DestroyTexture(titleTexture);
        if (messageTexture) SDL_DestroyTexture(messageTexture);
        if (tryAgainTexture) SDL_DestroyTexture(tryAgainTexture);
        if (cancelTexture) SDL_DestroyTexture(cancelTexture);
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }
    int dialogW = 400, dialogH = 200;
    SDL_Rect dialogRect = {(WINDOW_WIDTH - dialogW) / 2, (WINDOW_HEIGHT - dialogH) / 2, dialogW, dialogH};
    int titleW, titleH, messageW, messageH;
    SDL_QueryTexture(titleTexture, NULL, NULL, &titleW, &titleH);
    SDL_QueryTexture(messageTexture, NULL, NULL, &messageW, &messageH);
    SDL_Rect titleRect = {dialogRect.x + (dialogW - titleW) / 2, dialogRect.y + 20, titleW, titleH};
    SDL_Rect messageRect = {dialogRect.x + (dialogW - messageW) / 2, dialogRect.y + 60, messageW, messageH};
    int buttonW = 150, buttonH = 40;
    SDL_Rect tryAgainRect = {dialogRect.x + 40, dialogRect.y + 120, buttonW, buttonH};
    SDL_Rect cancelRect = {dialogRect.x + 220, dialogRect.y + 120, buttonW, buttonH}; 
    int tryAgainW, tryAgainH, cancelW, cancelH;
    SDL_QueryTexture(tryAgainTexture, NULL, NULL, &tryAgainW, &tryAgainH);
    SDL_QueryTexture(cancelTexture, NULL, NULL, &cancelW, &cancelH);
    SDL_Rect tryAgainTextRect = {tryAgainRect.x + (buttonW - tryAgainW) / 2, tryAgainRect.y + (buttonH - tryAgainH) / 2, tryAgainW, tryAgainH};
    SDL_Rect cancelTextRect = {cancelRect.x + (buttonW - cancelW) / 2, cancelRect.y + (buttonH - cancelH) / 2, cancelW, cancelH};
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
                if (SDL_PointInRect(&(SDL_Point){x, y}, &tryAgainRect)) {
                    result = DIALOG_RESULT_TRY_AGAIN;
                    inDialog = false;
                } else if (SDL_PointInRect(&(SDL_Point){x, y}, &cancelRect)) {
                    result = DIALOG_RESULT_CANCEL;
                    inDialog = false;
                }
            }
        }
        SDL_SetRenderDrawColor(game->pRenderer, 50, 50, 50, 255);
        SDL_RenderFillRect(game->pRenderer, &dialogRect);
        SDL_SetRenderDrawColor(game->pRenderer, 100, 100, 100, 255);
        SDL_RenderFillRect(game->pRenderer, &tryAgainRect);
        SDL_RenderFillRect(game->pRenderer, &cancelRect);
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(game->pRenderer, &tryAgainRect);
        SDL_RenderDrawRect(game->pRenderer, &cancelRect);
        SDL_RenderCopy(game->pRenderer, titleTexture, NULL, &titleRect);
        SDL_RenderCopy(game->pRenderer, messageTexture, NULL, &messageRect);
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


bool connectToServer(Game* game, const char* ip, bool *timedOut) {
    IPaddress serverIP;
    if (SDLNet_ResolveHost(&serverIP, ip, SERVER_PORT) == -1) {
        SDL_Log("SDLNet_ResolveHost: %s", SDLNet_GetError());
        return false;
    }
    game->serverAddress = serverIP;
    game->pSocket = SDLNet_UDP_Open(0);
    if (!game->pSocket) {
        SDL_Log("SDLNet_UDP_Open: %s", SDLNet_GetError());
        return false;
    }
    game->pPacket = SDLNet_AllocPacket(sizeof(ServerData));
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
    bool gotConnectResponse = false;
    while (SDL_GetTicks() - start < 3000) {
        if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
            ServerCommand command;
            memcpy(&command, game->pPacket->data, sizeof(ServerCommand));
            if (!gotConnectResponse && game->pPacket->len == sizeof(ClientData)) {
                ClientData response;
                memcpy(&response, game->pPacket->data, sizeof(ClientData));
                if (response.command == CONNECT) {
                    game->playerNumber = response.playerNumber;
                    SDL_Log("Connected as player %d", game->playerNumber);
                    gotConnectResponse = true;
                }
            }
            if (command == START_MATCH && game->pPacket->len == sizeof(GameInitData)) {
                GameInitData initData;
                memcpy(&initData, game->pPacket->data, sizeof(GameInitData));
                game->playerNumber = initData.playerID;
                return true;
            }
        }
        SDL_Delay(10);
    }
    *timedOut = true;
    SDL_Log("ERROR: Timeout – inget START_MATCH mottaget");
    return false;
}


void receiveGameState(Game* game) {
    if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
        ServerCommand command;
        memcpy(&command, game->pPacket->data, sizeof(ServerCommand));
        if (command == GAME_STATE && game->pPacket->len == sizeof(ServerData)) {
            ServerData serverData;
            memcpy(&serverData, game->pPacket->data, sizeof(ServerData));
            game->numOtherTanks = 0;

            for (int i = 0; i < serverData.numPlayers; i++) {
                if (serverData.tanks[i].playerNumber == game->playerNumber) {
                    if (!game->tank) {
                        game->tank = createTank();
                        if (game->tank) {
                            SDL_Log("INFO: Clients tank created, player number = %d", game->playerNumber);
                        } else {
                            SDL_Log("ERROR: createTank() returned NULL!");
                        }
                    }
                    setTankPosition(game->tank, serverData.tanks[i].x, serverData.tanks[i].y);
                    setTankAngle(game->tank, serverData.tanks[i].angle);
                    setTankColorId(game->tank, serverData.tanks[i].tankColorId);
                    setTankHealth(game->tank, serverData.tanks[i].health);
                } else {
                    game->otherTanks[game->numOtherTanks++] = serverData.tanks[i];
                }
            }
            for (int i = 0; i < serverData.numBullets && i < MAX_PLAYERS * MAX_BULLETS_PER_PLAYER; i++) {
                Bullet* b = &game->bullets[i];
                b->rect.x = serverData.bullets[i].x;
                b->rect.y = serverData.bullets[i].y;
                b->rect.w = 15;
                b->rect.h = 15;
                b->velocityX = serverData.bullets[i].vx;
                b->velocityY = serverData.bullets[i].vy;
                b->active = serverData.bullets[i].active;
                b->ownerId = serverData.bullets[i].ownerId;
            }
            for (int i = serverData.numBullets; i < MAX_PLAYERS * MAX_BULLETS_PER_PLAYER; i++) {
                game->bullets[i].active = false;
            }
        } else if (command == MATCH_OVER && game->pPacket->len == sizeof(ServerData)) {
            ServerData serverData;
            memcpy(&serverData, game->pPacket->data, sizeof(ServerData));
            game->matchOver = true;
            game->winningPlayerID = serverData.winningPlayerID;
            SDL_Log("Match over, winner is Player %d", game->winningPlayerID);
        } else {
            SDL_Log("WARN: Unknown command (command=%d, len=%d)", command, game->pPacket->len);
        }
    }
}


void showWinnerDialog(Game* game, int winnerID) {
    TTF_Font* font = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 36); 
    TTF_Font* smallFont = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 25); 
    TTF_Font* okFont = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 22); 

    if (!font || !smallFont || !okFont) {
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
        if (font) TTF_CloseFont(font);
        if (smallFont) TTF_CloseFont(smallFont);
        if (okFont) TTF_CloseFont(okFont);
        return;
    }
    char message[32];
    snprintf(message, sizeof(message), "Player %d Wins", winnerID);
    SDL_Color textColor = {255, 255, 255, 255};
    SDL_Surface* messageSurface = TTF_RenderText_Solid(smallFont, message, textColor);
    if (!messageSurface) {
        SDL_Log("Failed to render message text: %s", TTF_GetError());
        TTF_CloseFont(font);
        TTF_CloseFont(smallFont);
        TTF_CloseFont(okFont);
        return;
    }
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(game->pRenderer, messageSurface);
    SDL_FreeSurface(messageSurface);
    if (!messageTexture) {
        SDL_Log("Failed to create message texture: %s", SDL_GetError());
        TTF_CloseFont(font);
        TTF_CloseFont(smallFont);
        TTF_CloseFont(okFont);
        return;
    }
    SDL_Surface* gameOverSurface = TTF_RenderText_Solid(font, "GAME OVER", textColor);
    if (!gameOverSurface) {
        SDL_Log("Failed to render GAME OVER text: %s", TTF_GetError());
        SDL_DestroyTexture(messageTexture);
        TTF_CloseFont(font);
        TTF_CloseFont(smallFont);
        TTF_CloseFont(okFont);
        return;
    }
    SDL_Texture* gameOverTexture = SDL_CreateTextureFromSurface(game->pRenderer, gameOverSurface);
    SDL_FreeSurface(gameOverSurface);
    if (!gameOverTexture) {
        SDL_Log("Failed to create GAME OVER texture: %s", SDL_GetError());
        SDL_DestroyTexture(messageTexture);
        TTF_CloseFont(font);
        TTF_CloseFont(smallFont);
        TTF_CloseFont(okFont);
        return;
    }
    SDL_Surface* okSurface = TTF_RenderText_Solid(okFont, "OK", textColor);
    if (!okSurface) {
        SDL_Log("Failed to render OK text: %s", TTF_GetError());
        SDL_DestroyTexture(messageTexture);
        SDL_DestroyTexture(gameOverTexture);
        TTF_CloseFont(font);
        TTF_CloseFont(smallFont);
        TTF_CloseFont(okFont);
        return;
    }
    SDL_Texture* okTexture = SDL_CreateTextureFromSurface(game->pRenderer, okSurface);
    SDL_FreeSurface(okSurface);
    if (!okTexture) {
        SDL_Log("Failed to create OK texture: %s", SDL_GetError());
        SDL_DestroyTexture(messageTexture);
        SDL_DestroyTexture(gameOverTexture);
        TTF_CloseFont(font);
        TTF_CloseFont(smallFont);
        TTF_CloseFont(okFont);
        return;
    }
    int dialogW = 400, dialogH = 200; 
    SDL_Rect dialogRect = {(WINDOW_WIDTH - dialogW) / 2, (WINDOW_HEIGHT - dialogH) / 2, dialogW, dialogH};
    int gameOverW, gameOverH;
    SDL_QueryTexture(gameOverTexture, NULL, NULL, &gameOverW, &gameOverH);
    SDL_Rect gameOverRect = {dialogRect.x + (dialogW - gameOverW) / 2, dialogRect.y + 20, gameOverW, gameOverH};
    int messageW, messageH;
    SDL_QueryTexture(messageTexture, NULL, NULL, &messageW, &messageH);
    SDL_Rect messageRect = {dialogRect.x + (dialogW - messageW) / 2, dialogRect.y + 20 + gameOverH + 10, messageW, messageH};
    int buttonW = 60, buttonH = 30;
    SDL_Rect okButtonRect = {dialogRect.x + (dialogW - buttonW) / 2, dialogRect.y + dialogH - buttonH - 20, buttonW, buttonH};
    int okW, okH;
    SDL_QueryTexture(okTexture, NULL, NULL, &okW, &okH);
    SDL_Rect okTextRect = {okButtonRect.x + (buttonW - okW) / 2, okButtonRect.y + (buttonH - okH) / 2, okW, okH};
    Uint32 startTime = SDL_GetTicks();
    bool showing = true;
    while (showing) {
        while (SDL_PollEvent(&game->event)) {
            if (game->event.type == SDL_QUIT) {
                game->state = STATE_EXIT;
                showing = false;
            } else if (game->event.type == SDL_MOUSEBUTTONDOWN) {
                int x = game->event.button.x;
                int y = game->event.button.y;
                if (SDL_PointInRect(&(SDL_Point){x, y}, &okButtonRect)) {
                    showing = false;
                }
            }
        }
        SDL_SetRenderDrawColor(game->pRenderer, 50, 50, 50, 255);
        SDL_RenderFillRect(game->pRenderer, &dialogRect);
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(game->pRenderer, &dialogRect);
        SDL_RenderCopy(game->pRenderer, gameOverTexture, NULL, &gameOverRect);
        SDL_RenderCopy(game->pRenderer, messageTexture, NULL, &messageRect);
        SDL_SetRenderDrawColor(game->pRenderer, 100, 100, 100, 255);
        SDL_RenderFillRect(game->pRenderer, &okButtonRect);
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(game->pRenderer, &okButtonRect);
        SDL_RenderCopy(game->pRenderer, okTexture, NULL, &okTextRect); 
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }
    SDL_DestroyTexture(messageTexture);
    SDL_DestroyTexture(gameOverTexture);
    SDL_DestroyTexture(okTexture);
    TTF_CloseFont(font);
    TTF_CloseFont(smallFont);
    TTF_CloseFont(okFont);
}

void showYouDiedDialog(Game* game) {
    TTF_Font* font = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 36);
    TTF_Font* okFont = TTF_OpenFont("../lib/resources/Orbitron-Bold.ttf", 22);

    if (!font || !okFont) {
        SDL_Log("TTF_OpenFont failed: %s", TTF_GetError());
        if (font) TTF_CloseFont(font);
        if (okFont) TTF_CloseFont(okFont);
        return;
    }

    SDL_Color textColor = {255, 255, 255, 255};

    SDL_Surface* diedSurface = TTF_RenderText_Solid(font, "YOU DIED", textColor);
    SDL_Texture* diedTexture = SDL_CreateTextureFromSurface(game->pRenderer, diedSurface);
    SDL_FreeSurface(diedSurface);

    SDL_Surface* okSurface = TTF_RenderText_Solid(okFont, "OK", textColor);
    SDL_Texture* okTexture = SDL_CreateTextureFromSurface(game->pRenderer, okSurface);
    SDL_FreeSurface(okSurface);

    int dialogW = 400, dialogH = 200;
    SDL_Rect dialogRect = { (WINDOW_WIDTH - dialogW) / 2, (WINDOW_HEIGHT - dialogH) / 2, dialogW, dialogH };

    int diedW, diedH;
    SDL_QueryTexture(diedTexture, NULL, NULL, &diedW, &diedH);
    SDL_Rect diedRect = { dialogRect.x + (dialogW - diedW) / 2, dialogRect.y + 50, diedW, diedH };

    int buttonW = 60, buttonH = 30;
    SDL_Rect okButtonRect = { dialogRect.x + (dialogW - buttonW) / 2, dialogRect.y + dialogH - buttonH - 20, buttonW, buttonH };

    int okW, okH;
    SDL_QueryTexture(okTexture, NULL, NULL, &okW, &okH);
    SDL_Rect okTextRect = { okButtonRect.x + (buttonW - okW) / 2, okButtonRect.y + (buttonH - okH) / 2, okW, okH };

    bool showing = true;
    while (showing) {
        while (SDL_PollEvent(&game->event)) {
            if (game->event.type == SDL_QUIT) {
                game->state = STATE_EXIT;
                showing = false;
            } else if (game->event.type == SDL_MOUSEBUTTONDOWN) {
                int x = game->event.button.x;
                int y = game->event.button.y;
                if (SDL_PointInRect(&(SDL_Point){x, y}, &okButtonRect)) {
                    showing = false;
                }
            }
        }

        SDL_SetRenderDrawColor(game->pRenderer, 50, 50, 50, 255);
        SDL_RenderFillRect(game->pRenderer, &dialogRect);
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(game->pRenderer, &dialogRect);

        SDL_RenderCopy(game->pRenderer, diedTexture, NULL, &diedRect);

        SDL_SetRenderDrawColor(game->pRenderer, 100, 100, 100, 255);
        SDL_RenderFillRect(game->pRenderer, &okButtonRect);
        SDL_SetRenderDrawColor(game->pRenderer, 255, 255, 255, 255);
        SDL_RenderDrawRect(game->pRenderer, &okButtonRect);
        SDL_RenderCopy(game->pRenderer, okTexture, NULL, &okTextRect);

        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(diedTexture);
    SDL_DestroyTexture(okTexture);
    TTF_CloseFont(font);
    TTF_CloseFont(okFont);
}


void sendClientUpdate(Game* game) {
    if (!game || !game->tank || !game->pSocket || !game->pPacket) return;
    ClientData data;
    memset(&data, 0, sizeof(ClientData));
    data.command = UPDATE;
    data.playerNumber = game->playerNumber;
    data.tankColorId = getTankColorId(game->tank);
    data.angle = getTankAngle(game->tank);
    const Uint8* keys = SDL_GetKeyboardState(NULL);
    data.up    = keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP];
    data.down  = keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN];
    data.left  = keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT];
    data.right = keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT];
    Uint32 now = SDL_GetTicks();
    if (keys[SDL_SCANCODE_SPACE] && (now - game->lastshottime > 700)) {
        data.shooting = true;
        game->lastshottime = now;
    } else {
        data.shooting = false;
    }
    memcpy(game->pPacket->data, &data, sizeof(ClientData));
    game->pPacket->len = sizeof(ClientData);
    SDLNet_UDP_Send(game->pSocket, -1, game->pPacket);
}


void closeGame(Game *game)
{
    if (game->tank) {
        destroyTankInstance(game->tank);
        game->tank = NULL;
    }
    destroyTank();
    destroyBulletTexture();
    destroyHeartTexture();
    if (game->topLeft) {
        destroyWall(game->topLeft);
        game->topLeft = NULL;
    }
    if (game->topRight) {
        destroyWall(game->topRight);
        game->topRight = NULL;
    }
    if (game->bottomLeft) {
        destroyWall(game->bottomLeft);
        game->bottomLeft = NULL;
    }
    if (game->bottomRight) {
        destroyWall(game->bottomRight);
        game->bottomRight = NULL;
    }
    for (int i = 0; i < MAXTANKS; i++) {
        if (game->tankTextures[i]) {
            SDL_DestroyTexture(game->tankTextures[i]);
            game->tankTextures[i] = NULL;
        }
    }
    if (game->pTankpicture != NULL) {
        SDL_DestroyTexture(game->pTankpicture);
        game->pTankpicture = NULL;
    }
    if (game->pBackground != NULL) {
        SDL_DestroyTexture(game->pBackground);
        game->pBackground = NULL;
    }
    if (game->pRenderer != NULL) {
        SDL_DestroyRenderer(game->pRenderer);
        game->pRenderer = NULL;
    }
    if (game->pWindow != NULL) {
        SDL_DestroyWindow(game->pWindow);
        game->pWindow = NULL;
    }
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
