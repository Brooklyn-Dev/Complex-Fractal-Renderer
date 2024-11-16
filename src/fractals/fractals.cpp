#include <algorithm>
#include <cmath>

#include "fractals.hpp"

const unsigned int PERIODICITY_ITERATION = 20;
const float PERIODICITY_EPSILON = 1e-8;

const float NEWTON_FRACTAL_EPSILON = 1e-6;
const Complex NEWTON_FRACTAL_ROOTS[3] = {
    Complex(1.0),
    Complex(-0.5, sqrt(3) / 2),
    Complex(-0.5, -sqrt(3) / 2 ),
};
const colour NEWTON_FRACTAL_COLOURS[3] = {
    colour{ 255, 0, 0 },
    colour{ 0, 255, 0 },
    colour{ 0, 0, 255 },
};

Complex screenToFractal(
    unsigned int px, unsigned int py,
    float halfWinWidth, float halfWinHeight,
    double fractalWidthRatio, double fractalHeightRatio,
    long double offsetX, long double offsetY
) {
    long double real = (px - halfWinWidth) * fractalWidthRatio + offsetX;
    long double imag = (halfWinHeight - py) * fractalHeightRatio + offsetY;

    return Complex(real, imag);
}

std::pair<unsigned int, unsigned int> fractalToScreen(
    Complex z,
    float halfWinWidth, float halfWinHeight,
    double fractalWidthRatio, double fractalHeightRatio,
    long double offsetX, long double offsetY
) {
    unsigned int px = (z.real() - offsetX) / fractalWidthRatio + halfWinWidth;
    unsigned int py = (-z.imag() + offsetY) / fractalHeightRatio + halfWinHeight;

    return std::make_pair(px, py);
}

int calculateIterations(unsigned int numZooms, unsigned int initialIterations, unsigned int iterationIncrement, unsigned int maxIterations) {
    return std::clamp<unsigned int>(numZooms * iterationIncrement + initialIterations, 1, maxIterations);
}

bool checkPeriodicity(const Complex& z, const Complex& prevZ) {
    long double deltaMagSq = Complex::magSq(z - prevZ);

    // Approximate the angle difference
    long double dot = z.real() * prevZ.real() + z.imag() * prevZ.imag();
    long double cross = z.real() * prevZ.imag() - z.imag() * prevZ.real();

    // Check if the magnitude and the angle are nearly the same
    return deltaMagSq < PERIODICITY_EPSILON && fabs(dot - 1) < PERIODICITY_EPSILON && fabs(cross) < PERIODICITY_EPSILON;
}

colour processMandelbrot(Complex c, unsigned int maxIterations) {
    // Check if inside main cardioid
    long double reMinusQuarter = c.real() - 0.25;
    long double imSquared = c.imag() * c.imag();
    long double q = reMinusQuarter * reMinusQuarter + imSquared;
    if (q * (q + reMinusQuarter) <= 0.25 * imSquared)
        return BLACK;

    // Check if inside period-2 bulb
    long double rePlusOne = c.real() + 1.0;
    if (rePlusOne * rePlusOne + imSquared <= 0.0625)
        return BLACK;

    Complex z = Complex(); // z_0 = 0
    Complex prevZ = z;

    for (int i = 0; i < maxIterations; i++) {
        z = z * z + c; // z_n+1 = z_n^2 + c

        // Escape condition
        if (Complex::magSq(z) > 4.0)
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

std::vector<Complex> calcTrajectoryMandelbrot(Complex c, unsigned int maxIterations) {
    std::vector<Complex> trajectory;
    Complex z = Complex();

    for (int i = 0; i < maxIterations; i++) {
        z = z * z + c;
        trajectory.push_back(z);

        if (Complex::magSq(z) > 4.0)
            break;
    }

    return trajectory;
}

colour processTricorn(Complex c, unsigned int maxIterations) {
    Complex z = Complex(); // z_0 = 0
    Complex prevZ = z;

    for (int i = 0; i < maxIterations; i++) {
        Complex zConj = Complex::conj(z);
        z = zConj * zConj + c; // z_n+1 = Conj(z)_n^2 + c

        // Escape condition
        if (Complex::magSq(z) > 4.0)
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

std::vector<Complex> calcTrajectoryTricorn(Complex c, unsigned int maxIterations) {
    std::vector<Complex> trajectory;
    Complex z = Complex();

    for (int i = 0; i < maxIterations; i++) {
        Complex zConj = Complex::conj(z);
        z = zConj * zConj + c;
        trajectory.push_back(z);

        if (Complex::magSq(z) > 4.0)
            break;
    }

    return trajectory;
}

colour processBurningShip(Complex c, unsigned int maxIterations) {
    Complex z = Complex(); // z_0 = 0
    Complex prevZ = z;

    c = Complex::conj(c); // Reflect in real axis

    for (int i = 0; i < maxIterations; i++) {
        long double absReal = abs(z.real());
        long double absImag = abs(z.imag());
        z = Complex(absReal, absImag) * Complex(absReal, absImag) + c; // z_n+1 = (|Re(z_n)| + i|Im(z_n)|)^2 + c

        // Escape condition
        if (Complex::magSq(z) > 4.0)
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

std::vector<Complex> calcTrajectoryBurningShip(Complex c, unsigned int maxIterations) {
    std::vector<Complex> trajectory;
    Complex z = Complex();

    c = Complex::conj(c);

    for (int i = 0; i < maxIterations; i++) {
        long double absReal = abs(z.real());
        long double absImag = abs(z.imag());
        z = Complex(absReal, absImag) * Complex(absReal, absImag) + c;

        trajectory.push_back(Complex::conj(z));

        if (Complex::magSq(z) > 4.0)
            break;
    }

    return trajectory;
}

colour processNewtonFractal(Complex z, unsigned int maxIterations) {
    for (int i = 0; i < maxIterations; i++) {
        Complex zSquared = z * z; // z^2
        Complex zCubed = zSquared * z; // z^3
        Complex fz = zCubed - 1.0; // f(z) = z^3 - 1
        Complex fzPrime = zSquared * 3.0; // f'(z) = 3z^2

        z -= fz / fzPrime; // z_n+1 = z_n - f(z) / f'(z)

        // Assign colour based on which root z converges to
        for (int j = 0; j < 3; j++)
        {
            Complex diff = z - NEWTON_FRACTAL_ROOTS[j];
            if (abs(diff.real()) < NEWTON_FRACTAL_EPSILON && abs(diff.imag()) < NEWTON_FRACTAL_EPSILON)
                return NEWTON_FRACTAL_COLOURS[j];
        };
    }

    return BLACK;
}

std::vector<Complex> calcTrajectoryNewtonFractal(Complex z, unsigned int maxIterations) {
    std::vector<Complex> trajectory;

    for (int i = 0; i < maxIterations; i++) {
        trajectory.push_back(z);

        Complex zSquared = z * z;
        Complex zCubed = zSquared * z;
        Complex fz = zCubed - 1.0;
        Complex fzPrime = zSquared * 3.0;

        if (Complex::magSq(fz) < NEWTON_FRACTAL_EPSILON)
            break;

        z = z - fz / fzPrime;
    }

    return trajectory;
}