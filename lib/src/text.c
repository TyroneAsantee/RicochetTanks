#include "text.h"

static TTF_Font* font = NULL;

void initTextSystem(const char* fontPath, int fontSize) {
    if (TTF_Init() == -1) {
        SDL_Log("TTF_Init Error: %s", TTF_GetError());
        return;
    }
    font = TTF_OpenFont(fontPath, fontSize);
    if (!font) {
        SDL_Log("Failed to load font %s: %s", fontPath, TTF_GetError());
    }
}

void renderText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) {
    if (!font) return;

    SDL_Surface* surface = TTF_RenderText_Blended(font, text, color);
    if (!surface) {
        SDL_Log("Text Surface Error: %s", TTF_GetError());
        return;
    }

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (!texture) {
        SDL_Log("Text Texture Error: %s", SDL_GetError());
        SDL_FreeSurface(surface);
        return;
    }

    SDL_Rect destRect = { x, y, surface->w, surface->h };

    SDL_FreeSurface(surface);
    SDL_RenderCopy(renderer, texture, NULL, &destRect);
    SDL_DestroyTexture(texture);
}

void closeTextSystem(void) {
    if (font) {
        TTF_CloseFont(font);
        font = NULL;
    }
    TTF_Quit();
}
