#ifndef FRACTAL_OPTION_H
#define FRACTAL_OPTION_H

#include <SDL2/SDL.h>

#include "../complex/complex.hpp"
#include "../colour/colour.hpp"

typedef struct fractalOption {
    std::string name;
    SDL_Keycode key;
    std::function<colour(complex, int)> func;
    std::function<std::vector<complex>(complex, int)> trajectoryFunc;
};

#endif