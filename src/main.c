#include <stdio.h>
#include <SDL.h>
#include <SDL_image.h>
#include <stdbool.h>
#include "tank.h"
#include "timer.h"

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

int main(int argv, char** args){
    if(SDL_Init(SDL_INIT_VIDEO)!=0){
        printf("Error: %s\n",SDL_GetError());
        return 1;
    }
}

