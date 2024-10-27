#ifndef COLOUR_H
#define COLOUR_H

typedef struct colour {
    unsigned char r;
    unsigned char g;
    unsigned char b;
};

colour colourLerp(colour a, colour b, float t);
colour colourGradient(unsigned int iteration, unsigned int maxIterations);

const colour BLACK = colour{ 0, 0, 0 };

#endif