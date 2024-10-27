#include <algorithm>
#include <cmath>

#include "fractals.hpp"

const unsigned int PERIODICITY_ITERATION = 20;
const float PERIODICITY_EPSILON = 1e-8;

const float NEWTON_FRACTAL_EPSILON = 1e-6;
const complex NEWTON_FRACTAL_ROOTS[3] = {
    complex{ 1, 0 },
    complex{ -0.5, sqrt(3) / 2 },
    complex{ -0.5, -sqrt(3) / 2 },
};
const colour NEWTON_FRACTAL_COLOURS[3] = {
    colour{ 255, 0, 0 },
    colour{ 0, 255, 0 },
    colour{ 0, 0, 255 },
};

complex screenToFractal(
    unsigned int px, unsigned int py,
    float halfWinWidth, float halfWinHeight,
    double fractalWidthRatio, double fractalHeightRatio,
    double offsetX, double offsetY
) {
    double real = (px - halfWinWidth) * fractalWidthRatio + offsetX;
    double imag = (halfWinHeight - py) * fractalHeightRatio + offsetY;

    return complex{ real, imag };
}

std::pair<double, double> fractalToScreen(
    complex z,
    float halfWinWidth, float halfWinHeight,
    double fractalWidthRatio, double fractalHeightRatio,
    double offsetX, double offsetY
) {
    double px = (z.re - offsetX) / fractalWidthRatio + halfWinWidth;
    double py = (offsetY - z.im) / fractalHeightRatio + halfWinHeight;

    return std::make_pair(px, py);
}

int calculateIterations(unsigned int numZooms, unsigned int initialIterations, unsigned int iterationIncrement) {
    return numZooms * iterationIncrement + initialIterations;
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

std::vector<complex> calcTrajectoryMandelbrot(complex c, unsigned int maxIterations) {
    std::vector<complex> trajectory;
    complex z = { 0, 0 };

    for (int i = 0; i < maxIterations; i++) {
        z = z * z + c;
        trajectory.push_back(z);

        if (complexMagSq(z) > 4.0)
            break;
    }

    return trajectory;
}

colour processTricorn(complex c, unsigned int maxIterations) {
    complex z = { 0, 0 }; // z_0 = 0
    complex prevZ = z;

    for (int i = 0; i < maxIterations; i++) {
        complex zConj = complexConj(z);
        z = zConj * zConj + c; // z_n+1 = Conj(z)_n^2 + c

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

std::vector<complex> calcTrajectoryTricorn(complex c, unsigned int maxIterations) {
    std::vector<complex> trajectory;
    complex z = { 0, 0 };

    for (int i = 0; i < maxIterations; i++) {
        complex zConj = complexConj(z);
        z = zConj * zConj + c;
        trajectory.push_back(z);

        if (complexMagSq(z) > 4.0)
            break;
    }

    return trajectory;
}

colour processBurningShip(complex c, unsigned int maxIterations) {
    complex z = { 0, 0 }; // z_0 = 0
    complex prevZ = z;

    c = complexConj(c); // Reflect in real axis

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

std::vector<complex> calcTrajectoryBurningShip(complex c, unsigned int maxIterations) {
    std::vector<complex> trajectory;
    complex z = { 0, 0 };

    c = complexConj(c);

    for (int i = 0; i < maxIterations; i++) { 
        z = complex{ fabs(z.re), fabs(z.im) } * complex{ fabs(z.re), fabs(z.im) } + c;
        trajectory.push_back(complexConj(z));

        if (complexMagSq(z) > 4.0)
            break;
    }

    return trajectory;
}

colour processNewtonFractal(complex z, unsigned int maxIterations) {
    for (int i = 0; i < maxIterations; i++) {
        complex zSquared = z * z; // z^2
        complex zCubed = zSquared * z; // z^3
        complex fz = zCubed - 1; // f(z) = z^3 - 1
        complex fzPrime = zSquared * 3; // f'(z) = 3z^2

        z -= fz / fzPrime; // z_n+1 = z_n - f(z) / f'(z)

        // Assign colour based on which root z converges to
        for (int j = 0; j < 3; j++)
        {
            complex diff = z - NEWTON_FRACTAL_ROOTS[j];
            if (fabs(diff.re) < NEWTON_FRACTAL_EPSILON && fabs(diff.im) < NEWTON_FRACTAL_EPSILON)
                return NEWTON_FRACTAL_COLOURS[j];
        };
    }

    return BLACK;
}

std::vector<complex> calcTrajectoryNewtonFractal(complex z, unsigned int maxIterations) {
    std::vector<complex> trajectory;

    for (int i = 0; i < maxIterations; i++) {
        trajectory.push_back(z);

        complex zSquared = z * z;
        complex zCubed = zSquared * z;
        complex fz = zCubed - 1;
        complex fzPrime = zSquared * 3;

        if (complexMagSq(fz) < NEWTON_FRACTAL_EPSILON)
            break;

        z = z - fz / fzPrime;
    }

    return trajectory;
}