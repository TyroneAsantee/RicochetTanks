#ifndef TIMER_H
#define TIMER_H
#include <SDL.h>
#include <SDL_image.h>

typedef struct {
    Uint32 last_time;
    float delta_time;
} Timer;

void initiate_timer(Timer* timer);
void update_timer(Timer* timer);
float get_timer(Timer* timer);

#endif