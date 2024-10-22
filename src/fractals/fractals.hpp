#ifndef FRACTALS_H
#define FRACTALS_H

#include "../complex/complex.hpp"
#include "../colour/colour.hpp"

complex screenToFractal(
	unsigned int px, unsigned int py,
	float halfWinWidth, float halfWinHeight,
	double fractalWidthRatio, double fractalHeightRatio,
	double offsetX, double offsetY
);
int calculateIterations(unsigned int numZooms, unsigned int baseIterations = 64);
bool checkPeriodicity(const complex& z, const complex& prevZ);
colour processMandelbrot(complex c, unsigned int maxIterations);
colour processTricorn(complex c, unsigned int maxIterations);
colour processBurningShip(complex c, unsigned int maxIterations);
colour processNewtonFractal(complex z, unsigned int maxIterations);

#endif