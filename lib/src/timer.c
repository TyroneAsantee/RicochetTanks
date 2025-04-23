#include "timer.h"

void initiate_timer(Timer* timer)                                           // Start timer
{
    timer->last_time = SDL_GetTicks();                                      // Return time since SDL started
    timer->delta_time = 0.0f;                                               // No time has gone, yet
}

void update_timer(Timer* timer)                                             // Update timer, done every frame
{
    Uint32 current_time = SDL_GetTicks();                                   // Get current time
    timer->delta_time = (current_time - timer->last_time) / 1000.0f;        // Difference between now and last update in seconds
    timer->last_time = current_time;                                        // Store current time for next time

    if(timer->delta_time > 0.05f)                                           // Sets max speed
    {
        timer->delta_time = 0.05f;
    }
}

float get_timer(Timer* timer)                                               // Return last measured time (seconds)
{
    return timer->delta_time;                                               // Can be used in main.c
}