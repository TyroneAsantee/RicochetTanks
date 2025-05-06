#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <math.h>
#include "tank.h"
#include "timer.h"
#include "bullet.h"
#include "collision.h"
#include "text.h"
#include "wall.h"
#include "client.h"
#include "network_protocol.h"
#include "game.h"

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
   DIALOG_RESULT_NONE,
   DIALOG_RESULT_TRY_AGAIN,
   DIALOG_RESULT_CANCEL
} DialogResult;



void initiate(Game* game);
bool connectToServer(Game* game, const char* ip, bool *timedOut);
void run(Game* game);
void runMainMenu(Game* game);
void enterServerIp(Game* game);
void selectTank(Game* game);
void loadSelectedTankTexture(Game* game);
void closeGame(Game* game);
bool waitForServerResponse(Game *game, Uint32 timeout_ms);
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
                SDL_Log("DEBUG: main() går in i run()");
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

   game->state = STATE_MENU;
   game->pPacket = NULL;
   game->pSocket = NULL;
   game->tankColorId = 0;
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
           renderText(game->pRenderer, "Type IP and press ENTER:", 175, 100, white);
        
            if (strlen(inputBuffer) > 0) {
                renderText(game->pRenderer, inputBuffer, 175, 150, white);
            }

           SDL_RenderPresent(game->pRenderer);
           SDL_Delay(16);
   }
   SDL_StopTextInput();
   SDL_DestroyTexture(background);
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
                    SDL_Delay(200);

                    // 🔁 Host ansluter till sin egen server
                    bool timedOut;
                    if (!connectToServer(game, game->ipAddress, &timedOut)) {
                        SDL_Log("Host kunde inte ansluta till sin egen server.");
                        return;
                    }
                    SDL_Texture* bgWait = IMG_LoadTexture(game->pRenderer, "../lib/resources/menu_bg.png");
                    bool waiting = true;

                    while (waiting && connectedPlayers < MAX_PLAYERS) {
                        while (SDL_PollEvent(&game->event)) {
                            if (game->event.type == SDL_QUIT || 
                                (game->event.type == SDL_KEYDOWN && game->event.key.keysym.sym == SDLK_ESCAPE)) {
                                game->state = STATE_MENU;
                                inMenu = true;
                                waiting = false;
                                break;
                            }

                            if (game->event.type == SDL_KEYDOWN && game->event.key.keysym.sym == SDLK_SPACE) {
                                //if (connectedPlayers >= 2) {
                                    loadSelectedTankTexture(game);
                                    game->state = STATE_RUNNING;
                                    SDL_Log("DEBUG: runMainMenu satte state till STATE_RUNNING");
                                    inMenu = false;
                                    waiting = false;
                                    break;
                                //}
                            }
                        }

                        SDL_RenderClear(game->pRenderer);
                        SDL_RenderCopy(game->pRenderer, bgWait, NULL, NULL);

                        SDL_Color white = {255, 255, 255, 255};
                        char statusText[64];
                        snprintf(statusText, sizeof(statusText), "Waiting for players... %d/%d players", connectedPlayers, MAX_PLAYERS);
                        renderText(game->pRenderer, statusText, 200, 300, white);
                        renderText(game->pRenderer, "Press ESC to cancel", 200, 360, white);
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
                        SDL_Log("INFO: Försöker ansluta till %s...", game->ipAddress);
                        if (connectToServer(game, game->ipAddress, &timedOut)) {
                            SDL_Log("INFO: Anslutning lyckades, går vidare till STATE_RUNNING");
                            loadSelectedTankTexture(game);
                            game->state = STATE_RUNNING;
                            SDL_Log("DEBUG: runMainMenu satte state till STATE_RUNNING");
                            inMenu = false;
                            tryConnect = false;
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
    SDL_Log("DEBUG: run() - innan getTankRect");
    SDL_Rect tankRect = getTankRect(game->tank);
    SDL_Log("DEBUG: tankRect: x=%d y=%d w=%d h=%d", tankRect.x, tankRect.y, tankRect.w, tankRect.h);

    float shipX = tankRect.x;
    float shipY = tankRect.y;
    
    SDL_Log("DEBUG: run() - innan getTankAngle");
    float angle = getTankAngle(game->tank);
    SDL_Log("DEBUG: run() - efter getTankAngle");

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
        receiveGameState(game);
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
                            if (SDL_GetTicks() - game->lastshottime > 200) {
                                for (int i = 0; i < MAX_BULLETS; i++) {
                                    if (!game->bullets[i].active) {
                                        fireBullet(&game->bullets[i], shipX + tankRect.w / 2, shipY + tankRect.h / 2, angle, game->playerNumber);
                                        game->lastshottime = SDL_GetTicks();
                                        break;
                                    }
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
                        case SDL_SCANCODE_ESCAPE:
                            game->state = STATE_MENU;
                            closeWindow = true;
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

        // Tank-kollision mot väggar
        SDL_Rect futureTank = getTankRect(game->tank);
        futureTank.x = (int)(shipX + shipVelocityX * dt);
        futureTank.y = (int)(shipY + shipVelocityY * dt);

        if (!wallCheckCollision(game->topLeft, &futureTank) &&
            !wallCheckCollision(game->topRight, &futureTank) &&
            !wallCheckCollision(game->bottomLeft, &futureTank) &&
            !wallCheckCollision(game->bottomRight, &futureTank)) {
            shipX += shipVelocityX * dt;
            shipY += shipVelocityY * dt;
        }

        if (shipX < 0) shipX = 0;
        if (shipY < 0) shipY = 0;
        if (shipX > WINDOW_WIDTH - tankRect.w) shipX = WINDOW_WIDTH - tankRect.w;
        if (shipY > WINDOW_HEIGHT - tankRect.h) shipY = WINDOW_HEIGHT - tankRect.h;

        SDL_Log("DEBUG: run() - innan setTankPosition (%.2f, %.2f)", shipX, shipY);
        setTankPosition(game->tank, shipX, shipY);
        SDL_Log("DEBUG: run() - efter setTankPosition");


        setTankAngle(game->tank, angle);

        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);

        renderWall(game->pRenderer, game->topLeft);
        renderWall(game->pRenderer, game->topRight);
        renderWall(game->pRenderer, game->bottomLeft);
        renderWall(game->pRenderer, game->bottomRight);

        for (int i = 0; i < game->numOtherTanks; i++) {
            TankState t = game->otherTanks[i];
            SDL_Rect otherRect = { t.x, t.y, 64, 64 };

            int id = t.tankColorId;
            if (id < 0 || id >= MAXTANKS) id = 0;

            SDL_Texture* tex = game->tankTextures[id];
            SDL_RenderCopyEx(game->pRenderer, tex, NULL, &otherRect, t.angle, NULL, SDL_FLIP_NONE);
        }

        if (isTankAlive(game->tank)) {
            drawTank(game->pRenderer, game->tank, game->pTankpicture);
            renderTankHealth(game->pRenderer, getTankHealth(game->tank));
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
                    wallCheckCollision(game->topRight, &bulletRect) ||
                    wallCheckCollision(game->bottomLeft, &bulletRect) ||
                    wallCheckCollision(game->bottomRight, &bulletRect)) {

                    if (wallHitsVertical(game->topLeft, game->topRight, game->bottomLeft, game->bottomRight, &bulletRect)) {
                        game->bullets[i].velocityX *= -1;
                    }
                    if (wallHitsHorizontal(game->topLeft, game->topRight, game->bottomLeft, game->bottomRight, &bulletRect)) {
                        game->bullets[i].velocityY *= -1;
                    }
                }

                SDL_Rect tankRect = getTankRect(game->tank);
                SDL_FRect bulletFRect = game->bullets[i].rect;

                if (game->bullets[i].ownerId != game->playerNumber && checkCollision(&tankRect, &bulletFRect)) {
                    game->bullets[i].active = false;
                    // Här kan du också lägga till logik för att ta skada:
                    int currentHP = getTankHealth(game->tank);
                    setTankHealth(game->tank, currentHP - 1);
                }
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
    game->tankTextures[0] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank.png");
    game->tankTextures[1] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_lego.png");
    game->tankTextures[2] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_light.png");
    game->tankTextures[3] = IMG_LoadTexture(game->pRenderer, "../lib/resources/tank_dark.png");

    // Sätt spelarens egen textur baserat på val
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

    SDL_Texture* titleTexture = SDL_CreateTextureFromSurface(game->pRenderer, titleSurface);
    SDL_Texture* messageTexture = SDL_CreateTextureFromSurface(game->pRenderer, messageSurface);
    SDL_FreeSurface(titleSurface);
    SDL_FreeSurface(messageSurface);

    if (!titleTexture || !messageTexture) {
        SDL_Log("Failed to create textures from surfaces: %s", SDL_GetError());
        if (titleTexture) SDL_DestroyTexture(titleTexture);
        if (messageTexture) SDL_DestroyTexture(messageTexture);
        TTF_CloseFont(font);
        return DIALOG_RESULT_CANCEL;
    }

    int dialogW = 400, dialogH = 200;
    SDL_Rect dialogRect = {(800 - dialogW) / 2, (600 - dialogH) / 2, dialogW, dialogH};

    bool inDialog = true;
    DialogResult result = DIALOG_RESULT_NONE;

    while (inDialog) {
        while (SDL_PollEvent(&game->event)) {
            if (game->event.type == SDL_QUIT) {
                game->state = STATE_EXIT;
                inDialog = false;
                result = DIALOG_RESULT_CANCEL;
            } else if (game->event.type == SDL_MOUSEBUTTONDOWN) {
                inDialog = false;
                result = DIALOG_RESULT_CANCEL;
            }
        }

        SDL_SetRenderDrawColor(game->pRenderer, 50, 50, 50, 255);
        SDL_RenderFillRect(game->pRenderer, &dialogRect);
        SDL_RenderCopy(game->pRenderer, titleTexture, NULL, NULL);
        SDL_RenderCopy(game->pRenderer, messageTexture, NULL, NULL);
        SDL_RenderPresent(game->pRenderer);
        SDL_Delay(16);
    }

    SDL_DestroyTexture(titleTexture);
    SDL_DestroyTexture(messageTexture);
    TTF_CloseFont(font);
    return result;
}



void closeGame(Game *game)
{
    if (game->tank) destroyTankInstance(game->tank);
    destroyTank();
    destroyBulletTexture();
    destroyHeartTexture();
    destroyWall(game->topLeft);
    destroyWall(game->topRight);
    destroyWall(game->bottomLeft);
    destroyWall(game->bottomRight);

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

    if (game->pRenderer != NULL) SDL_DestroyRenderer(game->pRenderer);
    if (game->pWindow != NULL) SDL_DestroyWindow(game->pWindow);

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