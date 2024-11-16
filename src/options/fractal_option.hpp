#ifndef FRACTAL_OPTION_H
#define FRACTAL_OPTION_H

#include <SDL2/SDL.h>

#include "../complex/complex.hpp"
#include "../colour/colour.hpp"

struct fractalOption {
    std::string name;
    SDL_Keycode key;
    std::function<colour(Complex, unsigned int)> func;
    std::function<std::vector<Complex>(Complex, unsigned int)> trajectoryFunc;
};

#endif