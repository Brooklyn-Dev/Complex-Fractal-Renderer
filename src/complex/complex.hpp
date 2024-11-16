#ifndef COMPLEX_H
#define COMPLEX_H

class Complex {
    private:
        long double re;
        long double im;

    public:
        // Constructors
        Complex();
        Complex(const long double& value);
        Complex(const long double& real, const long double& imag);

        // Getters
        long double real() const;
        long double imag() const;

        // Complex arithmetic operators
        Complex operator+(const Complex& w) const;
        Complex operator-(const Complex& w) const;
        Complex operator*(const Complex& w) const;
        Complex operator/(const Complex& w) const;

        // Complex compound assignment operators
        Complex& operator+=(const Complex& w);
        Complex& operator-=(const Complex& w);
        Complex& operator*=(const Complex& w);
        Complex& operator/=(const Complex& w);

        // Scalar arithmetic operators
        Complex operator+(long double scalar) const;
        Complex operator-(long double scalar) const;
        Complex operator*(long double scalar) const;
        Complex operator/(long double scalar) const;

        // Scalar compound assignment operators
        Complex& operator+=(long double scalar);
        Complex& operator-=(long double scalar);
        Complex& operator*=(long double scalar);
        Complex& operator/=(long double scalar);

        // Mathmatical operations
        static Complex conj(const Complex& z);
        static long double mag(const Complex& z);
        static long double magSq(const Complex& z);
};

#endif