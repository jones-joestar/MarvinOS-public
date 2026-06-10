#pragma once

#define M_PI     3.14159265358979323846
#define M_PI_2   1.57079632679489661923
#define M_SQRT2  1.41421356237309504880
#define HUGE_VAL __builtin_huge_val()

double fabs(double x);
double sqrt(double x);
double floor(double x);
double ceil(double x);
double round(double x);
double sin(double x);
double cos(double x);
double tan(double x);
double atan(double x);
double atan2(double y, double x);
double pow(double base, double exp);
double log(double x);
double log2(double x);
double log10(double x);
double exp(double x);
double fmod(double x, double y);
