#ifndef CLIENT_H
#define CLIENT_H

#include <stdbool.h>
#include <SDL2/SDL_net.h>
#include <SDL2/SDL.h>  // För bool och Uint32
#include "text.h" // För DialogResult, eller flytta enum hit
#include "network_protocol.h"
#include "game.h"

#define MAX_PLAYERS 4

// Funktioner
bool connectToServer(Game* game, const char* ip, bool* timedOut);
bool waitForServerResponse(Game* game, Uint32 timeout_ms);
void receiveGameState(Game* game);  // Ny funktion från tidigare steg

#endif // CLIENT_H