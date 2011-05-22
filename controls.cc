#include "controls.hpp"
#include <SDL/SDL.h>

namespace eb {

void init_input() {
    SDL_EventState(SDL_MOUSEMOTION, SDL_IGNORE);
    
}

void update_input() {
     SDL_PumpEvents();
}


std::array<int, 3> get_mouse_buttons() {
    auto buttons = SDL_GetMouseState(NULL, NULL);
    return { { buttons&SDL_BUTTON(1),
                buttons&SDL_BUTTON(2),
                buttons&SDL_BUTTON(3) } };
}

int width = 800;
int height = 800;
point get_mouse()
{
    int x, y;
    
    SDL_GetMouseState(&x, &y);
    point result{};

    if(x >= 0 && 
       x < width && 
       y >=0 && y < height)
       result  = { x*1./width, y*1./height , 0.};
       else 
           result = { .5, .5, 0 };
       //std::cerr << result <<"\n";
       return result;
}
}
