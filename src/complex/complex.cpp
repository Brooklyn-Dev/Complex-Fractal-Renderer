#include <cmath>

#include "complex.hpp"

// Constructors
Complex::Complex() : re(0.0), im(0.0) {}
Complex::Complex(const long double& value) : re(value), im(0.0) {}
Complex::Complex(const long double& real, const long double& imag) : re(real), im(imag) {}

// Getters
long double Complex::real() const { return re; }
long double Complex::imag() const { return im; }

// Complex arithmetic operators
Complex Complex::operator+(const Complex& w) const {
    return Complex(re + w.re, im + w.im);
}

Complex Complex::operator-(const Complex& w) const {
    return Complex(re - w.re, im - w.im);
}

Complex Complex::operator*(const Complex& w) const {
    return Complex(re * w.re - im * w.im, re * w.im + im * w.re);
}

Complex Complex::operator/(const Complex& w) const {
    long double denom = w.re * w.re + w.im * w.im;
    return Complex((re * w.re + im * w.im) / denom, (im * w.re - re * w.im) / denom);
}

// Complex compound assignment operators
Complex& Complex::operator+=(const Complex& w) {
    re = re + w.re;
    im = im + w.im;
    return *this;
}

Complex& Complex::operator-=(const Complex& w) {
    re = re - w.re;
    im = im - w.im;
    return *this;
}

Complex& Complex::operator*=(const Complex& w) {
    long double newRe = re * w.re - im * w.im;
    im = re * w.im + im * w.re;
    re = newRe;
    return *this;
}

Complex& Complex::operator/=(const Complex& w) {
    long double denom = w.re * w.re + w.im * w.im;
    long double newRe = (re * w.re + im * w.im) / denom;
    im = (im * w.re - re * w.im) / denom;
    re = newRe;
    return *this;
}

// Scalar arithmetic operators
Complex Complex::operator+(long double scalar) const { return Complex(re + scalar, im); }
Complex Complex::operator-(long double scalar) const { return Complex(re - scalar, im); }
Complex Complex::operator*(long double scalar) const { return Complex(re * scalar, im * scalar); }
Complex Complex::operator/(long double scalar) const { return Complex(re / scalar, im / scalar); }

// Scalar compound assignment operators
Complex& Complex::operator+=(long double scalar) { re = re + scalar; return *this; }
Complex& Complex::operator-=(long double scalar) { re = re - scalar; return *this; }
Complex& Complex::operator*=(long double scalar) { re = re * scalar; im = im * scalar; return *this; }
Complex& Complex::operator/=(long double scalar) { re = re / scalar; im = im / scalar; return *this; }

// Mathmatical operations
Complex Complex::conj(const Complex& z) {
    return Complex(z.real(), -z.imag());
}

long double Complex::mag(const Complex& z) {
    return sqrtl(z.real() * z.real() + z.imag() * z.imag());
}

long double Complex::magSq(const Complex& z) {
    return z.real() * z.real() + z.imag() * z.imag();
}