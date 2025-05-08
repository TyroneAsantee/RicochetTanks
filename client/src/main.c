#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_net.h>
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

typedef enum {
    STATE_MENU,
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
} Game;


void initiate(Game* game);
bool connectToServer(Game* game, const char* ip, bool *timedOut);
void run(Game* game);
void runMainMenu(Game* game);
void enterServerIp(Game* game);
void selectTank(Game* game);
void loadSelectedTankTexture(Game* game);
void closeGame(Game* game);
bool connectToServer(Game* game, const char* ip, bool *timedOut);
void receiveGameState(Game* game);
void sendClientUpdate(Game* game);
DialogResult showErrorDialog(Game* game, const char* title, const char* message);


int main(int argv, char* args[]) {
   Game game;
   initiate(&game);

   while (game.state != STATE_EXIT) {
        SDL_Log("Main loop: game->state = %d", game.state);  //
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
                            loadSelectedTankTexture(game);
                            game->state = STATE_RUNNING;
                            inMenu = false;
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

void run(Game *game){
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
    SDL_Log("Inne i run()");

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
            float speed = SPEED * dt;
            float angleRad = getTankAngle(game->tank) * M_PI / 180.0f;
            float dx = cosf(angleRad) * speed;
            float dy = sinf(angleRad) * speed;

            SDL_Rect rect = getTankRect(game->tank);
            if (up) {
                rect.x += dx;
                rect.y += dy;
            }
            if (down) {
                rect.x -= dx;
                rect.y -= dy;
            }
            setTankPosition(game->tank, rect.x, rect.y);

            sendClientUpdate(game);
        }

        // RENDERING 
        SDL_RenderClear(game->pRenderer);
        SDL_RenderCopy(game->pRenderer, game->pBackground, NULL, NULL);

        // Rita andra spelares tanks
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

        // Rita din egen tank
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

    SDL_Log("Skickar ClientData med len=%d (ska vara %zu)", game->pPacket->len, sizeof(ClientData));
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
                SDL_Log("668Mottog START_MATCH – playerNumber: %d, arena: %dx%d",
                        initData.playerID, initData.arenaWidth, initData.arenaHeight);
                return true;
            }

            SDL_Log("Ignorerar inkommande paket – command=%d, len=%d", command, game->pPacket->len);
        }
        SDL_Delay(10);
    }

    *timedOut = true;
    SDL_Log("ERROR: Timeout – inget START_MATCH mottaget");
    return false;
}


void receiveGameState(Game* game) {
    if (SDLNet_UDP_Recv(game->pSocket, game->pPacket)) {
        if (game->pPacket->len < sizeof(ServerCommand)) {
            SDL_Log("WARN: Paket för kort (%d byte) för att ens innehålla ett ServerCommand", game->pPacket->len);
            return;
        }

        ServerCommand command;
        memcpy(&command, game->pPacket->data, sizeof(ServerCommand));

        if (command == GAME_STATE && game->pPacket->len == sizeof(ServerData)) {
            ServerData serverData;
            memcpy(&serverData, game->pPacket->data, sizeof(ServerData));

            SDL_Log("DEBUG: Mottaget GAME_STATE med %d spelare", serverData.numPlayers);
            game->numOtherTanks = 0;

            for (int i = 0; i < serverData.numPlayers; i++) {
                SDL_Log("DEBUG: Tank %d – playerNumber=%d (jag är %d)", i, serverData.tanks[i].playerNumber, game->playerNumber);

                if (serverData.tanks[i].playerNumber == game->playerNumber) {
                    if (!game->tank) {
                        game->tank = createTank();
                        SDL_Log("INFO: Klientens tank skapad (playerNumber=%d)", game->playerNumber);
                    }

                    setTankPosition(game->tank, serverData.tanks[i].x, serverData.tanks[i].y);
                    setTankAngle(game->tank, serverData.tanks[i].angle);
                    setTankColorId(game->tank, serverData.tanks[i].tankColorId);
                } else {
                    game->otherTanks[game->numOtherTanks++] = serverData.tanks[i];
                }
            }
        } else {
            SDL_Log("WARN: Mottog ogiltigt eller okänt kommando (command=%d, len=%d)", command, game->pPacket->len);
        }
    }
}

void sendClientUpdate(Game* game) {
    if (!game || !game->pSocket || !game->pPacket || !game->tank) return;

    ClientData data;
    memset(&data, 0, sizeof(ClientData));

    data.command = UPDATE;
    data.playerNumber = game->playerNumber;
    data.angle = getTankAngle(game->tank);
    data.up = false;
    data.down = false;
    data.shooting = false;
    data.tankColorId = game->tankColorId;

    const Uint8* keys = SDL_GetKeyboardState(NULL);
    if (keys[SDL_SCANCODE_W]) data.up = true;
    if (keys[SDL_SCANCODE_S]) data.down = true;

    memcpy(game->pPacket->data, &data, sizeof(ClientData));
    game->pPacket->len = sizeof(ClientData);
    game->pPacket->address = game->serverAddress;

    SDL_Log("Rad749:DEBUG: Skickar ClientData med len=%d (ska vara %zu)", game->pPacket->len, sizeof(ClientData));
    SDLNet_UDP_Send(game->pSocket, -1, game->pPacket);
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