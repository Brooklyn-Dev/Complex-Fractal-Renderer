#include <algorithm>
#include <cmath>

#include "fractals.hpp"

complex screenToFractal(unsigned int px, unsigned int py, unsigned int width, unsigned int height, double zoom, double offsetX, double offsetY) {
    double aspectRatio = width / (double)height;

    double fractalWidth, fractalHeight;

    if (width < height) {
        fractalWidth = 4.0 / zoom;   // -2 to 2 for the real axis
        fractalHeight = fractalWidth / aspectRatio;  // Scale imaginary axis proportionally
    }
    else {
        fractalHeight = 4.0 / zoom;  // -2i to 2i for the imaginary axis
        fractalWidth = fractalHeight * aspectRatio;  // Scale real axis proportionally
    }

    double real = (px - width / 2.0) * (fractalWidth / width) + offsetX;
    double imag = (py - height / 2.0) * (fractalHeight / height) + offsetY;

    return complex{ real, imag };
}

int calculateIterations(unsigned int numZooms, unsigned int baseIterations) {
    return numZooms * baseIterations + baseIterations;
}

colour processMandelbrot(complex c, unsigned int maxIterations) {
    complex z = { 0, 0 }; // z_0 = 0

    for (int i = 0; i < maxIterations; i++) {
        z = z * z + c; // z_n+1 = z_n^2 + c
        if (complexMagSq(z) > 4.0) // Escape condition
            return colourGradient(i, maxIterations);
    }

    return BLACK;
}

colour processTricorn(complex c, unsigned int maxIterations) {
    complex z = { 0, 0 }; // z_0 = 0

    for (int i = 0; i < maxIterations; i++) {
        complex zConjugate = complexConj(z);
        z = zConjugate * zConjugate + c; // z_n+1 = Conj(z)_n^2 + c
        if (complexMagSq(z) > 4.0)
            return colourGradient(i, maxIterations);
    }

    return BLACK;
}

colour processBurningShip(complex c, unsigned int maxIterations) {
    complex z = { 0, 0 }; // z_0 = 0

    for (int i = 0; i < maxIterations; i++) {
        z = complex{ fabs(z.re), fabs(z.im) } * complex{ fabs(z.re), fabs(z.im) } + c; // z_n+1 = (|Re(z_n)| + i|Im(z_n)|)^2 + c
        if (complexMagSq(z) > 4.0) // Escape condition
            return colourGradient(i, maxIterations);
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