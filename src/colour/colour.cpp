#include <math.h>

#include "colour.hpp"

// Linear interpolation between 2 colours
colour colourLerp(colour a, colour b, float t) {
    if (t > 1.0f) t = 1.0f;
    if (t < 0.0f) t = 0.0f;

    return colour{
        (unsigned char)(a.r + (b.r - a.r) * t),
        (unsigned char)(a.g + (b.g - a.g) * t),
        (unsigned char)(a.b + (b.b - a.b) * t)
    };
}

#define GRADIENT_STOP_COUNT 5
const colour GRADIENT_STOPS[GRADIENT_STOP_COUNT] = {
    { 50, 50, 255 }, // Blue
    { 50, 255, 50 }, // Green
    { 255, 255, 50 }, // Yellow
    { 255, 50, 50 }, // Red
    { 50, 50, 255 } // Loop back to Blue
};

// Get a colour from the gradient based on the iteration count
colour colourGradient(unsigned int iteration, unsigned int maxIterations) {
    int num_stops = GRADIENT_STOP_COUNT - 1;

    // Normalise the iteration count within the gradient range
    float gradient_position = fmodf(iteration / (float)maxIterations, 1.0f) * num_stops;

    // Determine the two stops to interpolate between
    int stop_prev = (int)floorf(gradient_position);
    int stop_next = stop_prev + 1;

    // Calculate the position between the two stops (0.0 to 1.0)
    float stop_fraction = gradient_position - stop_prev;

    // Lerp between the two stops
    return colourLerp(GRADIENT_STOPS[stop_prev], GRADIENT_STOPS[stop_next], stop_fraction);
}