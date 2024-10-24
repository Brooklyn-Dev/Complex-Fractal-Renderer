#include <stdlib.h>

#include "fractal_renderer/fractal_renderer.hpp"

const unsigned int WIN_WIDTH = 1600;
const unsigned int WIN_HEIGHT = 900;

int main(int argc, char* argv[]) {
    FractalRenderer renderer(WIN_WIDTH, WIN_HEIGHT);
    renderer.run();

    return EXIT_SUCCESS;
}