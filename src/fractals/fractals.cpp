#include <algorithm>
#include <cmath>

#include "fractals.hpp"

const unsigned int PERIODICITY_ITERATION = 20;
const double PERIODICITY_EPSILON = 1e-8;

complex screenToFractal(
    unsigned int px, unsigned int py,
    float halfWinWidth, float halfWinHeight,
    double fractalWidthRatio , double fractalHeightRatio,
    double offsetX, double offsetY
) {
    double real = (px - halfWinWidth) * fractalWidthRatio + offsetX;
    double imag = (py - halfWinHeight) * fractalHeightRatio + offsetY;

    return complex{ real, imag };
}

int calculateIterations(unsigned int numZooms, unsigned int baseIterations) {
    return numZooms * baseIterations + baseIterations;
}

bool checkPeriodicity(const complex& z, const complex& prevZ) {
    double deltaMagSq = complexMagSq(z - prevZ);

    // Approximate the angle difference
    double dot = z.re * prevZ.re + z.im * prevZ.im;
    double cross = z.re * prevZ.im - z.im * prevZ.re;

    // Check if the magnitude and the angle are nearly the same
    return deltaMagSq < PERIODICITY_EPSILON && fabs(dot - 1) < PERIODICITY_EPSILON && fabs(cross) < PERIODICITY_EPSILON;
}

colour processMandelbrot(complex c, unsigned int maxIterations) {
    // Check if inside main cardioid
    double reMinusQuarter = c.re - 0.25;
    double imSquared = c.im * c.im;
    double q = reMinusQuarter * reMinusQuarter + imSquared; 
    if (q * (q + reMinusQuarter) <= 0.25 * imSquared)
        return BLACK;

    // Check if inside period-2 bulb
    double rePlusOne = c.re + 1.0;
    if (rePlusOne * rePlusOne + imSquared <= 0.0625)
        return BLACK;

    complex z = { 0, 0 }; // z_0 = 0
    complex prevZ = z;

    for (int i = 0; i < maxIterations; i++) {
        z = z * z + c; // z_n+1 = z_n^2 + c

        // Escape condition
        if (complexMagSq(z) > 4.0) 
            return colourGradient(i, maxIterations);

        // Periodicity check
        if (i % PERIODICITY_ITERATION == 0) {
            if (checkPeriodicity(z, prevZ))
                return BLACK;

            prevZ = z;
        }
    }

    return BLACK;
}

colour processTricorn(complex c, unsigned int maxIterations) {
    complex z = { 0, 0 }; // z_0 = 0
    complex prevZ = z;

    for (int i = 0; i < maxIterations; i++) {
        complex zConjugate = complexConj(z);
        z = zConjugate * zConjugate + c; // z_n+1 = Conj(z)_n^2 + c

        // Escape condition
        if (complexMagSq(z) > 4.0)
            return colourGradient(i, maxIterations);

        // Periodicity check
        if (i % PERIODICITY_ITERATION == 0) {
            if (checkPeriodicity(z, prevZ))
                return BLACK;

            prevZ = z;
        }
    }

    return BLACK;
}

colour processBurningShip(complex c, unsigned int maxIterations) {
    complex z = { 0, 0 }; // z_0 = 0
    complex prevZ = z;

    for (int i = 0; i < maxIterations; i++) {
        z = complex{ fabs(z.re), fabs(z.im) } * complex{ fabs(z.re), fabs(z.im) } + c; // z_n+1 = (|Re(z_n)| + i|Im(z_n)|)^2 + c

        // Escape condition
        if (complexMagSq(z) > 4.0)
            return colourGradient(i, maxIterations);

        // Periodicity check
        if (i % PERIODICITY_ITERATION == 0) {
            if (checkPeriodicity(z, prevZ))
                return BLACK;

            prevZ = z;
        }
    }

    return BLACK;
}

colour processNewtonFractal(complex z, unsigned int maxIterations) {
    for (int i = 0; i < maxIterations; i++) {
        complex zSquared = z * z; // z^2
        complex zCubed = zSquared * z; // z^3
        complex fz = zCubed - 1; // f(z) = z^3 - 1
        complex fzPrime = zSquared * 3; // f'(z) = 3z^2

        if (complexMagSq(fz) < 1e-6) // Convergence check
            return colourGradient(i, maxIterations);

        z = z - fz / fzPrime; // z_n+1 = z_n - f(z) / f'(z)
    }

    return BLACK;
}