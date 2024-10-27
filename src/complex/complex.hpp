#ifndef COMPLEX_H
#define COMPLEX_H

#include <math.h>

typedef struct complex {
    long double re;
    long double im;

    complex operator+(const complex& w) const;
    complex& operator+=(const complex& w);

    complex operator-(const complex& w) const;
    complex& operator-=(const complex& w);

    complex operator*(const complex& w) const;
    complex& operator*=(const complex& w);

    complex operator/(const complex& w) const;
    complex& operator/=(const complex& w);

    complex operator+(long double scalar) const;
    complex& operator+=(long double scalar);

    complex operator-(long double scalar) const;
    complex& operator-=(long double scalar);

    complex operator*(long double scalar) const;
    complex& operator*=(long double scalar);

    complex operator/(long double scalar) const;
    complex& operator/=(long double scalar);
};

complex complexConj(const complex& z);
long double complexMag(const complex& z);
long double complexMagSq(const complex& z);

const complex I = complex{ 0, 1 };

#endif