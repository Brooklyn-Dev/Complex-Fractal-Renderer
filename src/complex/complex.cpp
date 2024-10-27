#include "complex.hpp"

complex complex::operator+(const complex& w) const {
    return complex{ re + w.re, im + w.im };
}
complex& complex::operator+=(const complex& w) {
    re += w.re;
    im += w.im;
    return *this;
}

complex complex::operator-(const complex& w) const {
    return complex{ re - w.re, im - w.im };
}
complex& complex::operator-=(const complex& w) {
    re -= w.re;
    im -= w.im;
    return *this;
}

complex complex::operator*(const complex& w) const {
    return complex{ re * w.re - im * w.im, re * w.im + im * w.re };
}
complex& complex::operator*=(const complex& w) {
    long double newRe = re * w.re - im * w.im;
    im = re * w.im + im * w.re;
    re = newRe;
    return *this;
}

complex complex::operator/(const complex& w) const {
    long double denominator = w.re * w.re + w.im * w.im;
    return complex{
        (re * w.re + im * w.im) / denominator,
        (im * w.re - re * w.im) / denominator
    };
}
complex& complex::operator/=(const complex& w) {
    long double denominator = w.re * w.re + w.im * w.im;
    long double newRe = (re * w.re + im * w.im) / denominator;
    im = (im * w.re - re * w.im) / denominator;
    re = newRe;
    return *this;
}

complex complex::operator+(long double scalar) const {
    return complex{ re + scalar, im };
}
complex& complex::operator+=(long double scalar) {
    re += scalar;
    return *this;
}

complex complex::operator-(long double scalar) const {
    return complex{ re - scalar, im };
}
complex& complex::operator-=(long double scalar) {
    re -= scalar;
    return *this;
}

complex complex::operator*(long double scalar) const {
    return complex{ re * scalar, im * scalar };
}
complex& complex::operator*=(long double scalar) {
    re *= scalar;
    im *= scalar;
    return *this;
}

complex complex::operator/(long double scalar) const {
    return complex{ re / scalar, im / scalar };
}
complex& complex::operator/=(long double scalar) {
    re /= scalar;
    im /= scalar;
    return *this;
}

complex complexConj(const complex& z) {
    return complex{ z.re, -z.im };
}

long double complexMag(const complex& z) {
    return sqrtl(z.re * z.re + z.im * z.im);
}

long double complexMagSq(const complex& z) {
    return z.re * z.re + z.im * z.im;
}