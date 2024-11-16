#ifndef FRACTALS_H
#define FRACTALS_H

#include <utility>
#include <vector>

#include "../complex/complex.hpp"
#include "../colour/colour.hpp"

Complex screenToFractal(
	unsigned int px, unsigned int py,
	float halfWinWidth, float halfWinHeight,
	double fractalWidthRatio, double fractalHeightRatio,
	long double offsetX, long double offsetY
);

std::pair<unsigned int, unsigned int> fractalToScreen(
	Complex z,
	float halfWinWidth, float halfWinHeight,
	double fractalWidthRatio, double fractalHeightRatio,
	long double offsetX, long double offsetY
);

int calculateIterations(unsigned int numZooms, unsigned int initialIterations, unsigned int iterationIncrement, unsigned int maxIterations);
bool checkPeriodicity(const Complex& z, const Complex& prevZ);

colour processMandelbrot(Complex c, unsigned int maxIterations);
std::vector<Complex> calcTrajectoryMandelbrot(Complex c, unsigned int maxIterations);

colour processTricorn(Complex c, unsigned int maxIterations);
std::vector<Complex> calcTrajectoryTricorn(Complex c, unsigned int maxIterations);

colour processBurningShip(Complex c, unsigned int maxIterations);
std::vector<Complex> calcTrajectoryBurningShip(Complex c, unsigned int maxIterations);

colour processNewtonFractal(Complex z, unsigned int maxIterations);
std::vector<Complex> calcTrajectoryNewtonFractal(Complex z, unsigned int maxIterations);

#endif