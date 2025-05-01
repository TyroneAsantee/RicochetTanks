#ifndef TEXT_H
#define TEXT_H
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

void initTextSystem(const char* fontPath, int fontSize);
void renderText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color);
void closeTextSystem(void);

#endif