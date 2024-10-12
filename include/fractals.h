#ifndef FRACTALS_H
#define FRACTALS_H

#include "../include/complex.h"
#include "../include/colour.h"

const int MAX_ITERATIONS = 128;

complex screenToFractal(int px, int py, int width, int height, double zoom, double offsetX, double offsetY);
colour processMandelbrot(complex c, int maxIterations);

#endif