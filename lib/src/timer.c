#include "timer.h"

void initiate_timer(Timer* timer)                                           
{
    timer->last_time = SDL_GetTicks();                                      
    timer->delta_time = 0.0f;                                               
}

void update_timer(Timer* timer)                                             
{
    Uint32 current_time = SDL_GetTicks();                                   
    timer->delta_time = (current_time - timer->last_time) / 1000.0f;        
    timer->last_time = current_time;                                        

    if(timer->delta_time > 0.05f)                                           
    {
        timer->delta_time = 0.05f;
    }
}

float get_timer(Timer* timer)                                               
{
    return timer->delta_time;                                              
}