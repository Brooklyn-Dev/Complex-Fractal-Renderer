#ifndef FRACTALS_H
#define FRACTALS_H

#include "../complex/complex.hpp"
#include "../colour/colour.hpp"

const int MAX_ITERATIONS = 128;

complex screenToFractal(int px, int py, int width, int height, double zoom, double offsetX, double offsetY);
colour processMandelbrot(complex c, int maxIterations);
colour processTricorn(complex c, int maxIterations);
colour processBurningShip(complex c, int maxIterations);
colour processNewtonFractal(complex z, int maxIterations);

#endif