/*
* sst-basic-blocks - a Surge Synth Team product
*
* Provides basic tools and blocks for use on the audio thread in
* synthesis, including basic DSP and modulation functions
*
* Copyright 2019 - 2023, Various authors, as described in the github
* transaction log.
*
* sst-basic-blocks is released under the Gnu General Public Licence
* V3 or later (GPL-3.0-or-later). The license is found in the file
* "LICENSE" in the root of this repository or at
* https://www.gnu.org/licenses/gpl-3.0.en.html
*
* All source for sst-basic-blocks is available at
* https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef SST_BASIC_BLOCKS_DSP_SPECIALFUNCTIONS_H
#define SST_BASIC_BLOCKS_DSP_SPECIALFUNCTIONS_H

#include <cstdint>
#include <cmath>

namespace sst::basic_blocks::dsp
{

inline double sincf(double x)
{
    if (x == 0)
    {
        return 1;
    }

    return (sin(M_PI * x)) / (M_PI * x);
}

inline double sinc(double x)
{
    if (fabs(x) < 0.0000000000000000000001)
    {
        return 1.0;
    }

    return (sin(x) / x);
}

inline double blackman(int i, int n)
{
    return (0.42 - 0.5 * cos(2 * M_PI * i / (n - 1)) + 0.08 * cos(4 * M_PI * i / (n - 1)));
}

inline double symmetric_blackman(double i, int n)
{
    i -= (n / 2);

    return (0.42 - 0.5 * cos(2 * M_PI * i / (n)) + 0.08 * cos(4 * M_PI * i / (n)));
}

inline double blackman(double i, int n)
{
    return (0.42 - 0.5 * cos(2 * M_PI * i / (n - 1)) + 0.08 * cos(4 * M_PI * i / (n - 1)));
}

inline double blackman_harris(int i, int n)
{
    return (0.35875 - 0.48829 * cos(2 * M_PI * i / (n - 1)) +
            0.14128 * cos(4 * M_PI * i / (n - 1)) - 0.01168 * cos(6 * M_PI * i / (n - 1)));
}

inline double symmetric_blackman_harris(double i, int n)
{
    i -= (n / 2);

    return (0.35875 - 0.48829 * cos(2 * M_PI * i / (n)) + 0.14128 * cos(4 * M_PI * i / (n - 1)) -
            0.01168 * cos(6 * M_PI * i / (n)));
}

inline double blackman_harris(double i, int n)
{
    return (0.35875 - 0.48829 * cos(2 * M_PI * i / (n - 1)) +
            0.14128 * cos(4 * M_PI * i / (n - 1)) - 0.01168 * cos(6 * M_PI * i / (n - 1)));
}

inline double hanning(int i, int n)
{
    if (i >= n)
    {
        return 0;
    }

    return 0.5 * (1 - cos(2 * M_PI * i / (n - 1)));
}

inline double hamming(int i, int n)
{
    if (i >= n)
    {
        return 0;
    }

    return 0.54 - 0.46 * cos(2 * M_PI * i / (n - 1));
}


inline double BESSI0(double X)
{
    double Y, P1, P2, P3, P4, P5, P6, P7, Q1, Q2, Q3, Q4, Q5, Q6, Q7, Q8, Q9, AX, BX;
    P1 = 1.0;
    P2 = 3.5156229;
    P3 = 3.0899424;
    P4 = 1.2067429;
    P5 = 0.2659732;
    P6 = 0.360768e-1;
    P7 = 0.45813e-2;
    Q1 = 0.39894228;
    Q2 = 0.1328592e-1;
    Q3 = 0.225319e-2;
    Q4 = -0.157565e-2;
    Q5 = 0.916281e-2;
    Q6 = -0.2057706e-1;
    Q7 = 0.2635537e-1;
    Q8 = -0.1647633e-1;
    Q9 = 0.392377e-2;
    if (fabs(X) < 3.75)
    {
        Y = (X / 3.75) * (X / 3.75);
        return (P1 + Y * (P2 + Y * (P3 + Y * (P4 + Y * (P5 + Y * (P6 + Y * P7))))));
    }
    else
    {
        AX = fabs(X);
        Y = 3.75 / AX;
        BX = exp(AX) / sqrt(AX);
        AX = Q1 +
             Y * (Q2 + Y * (Q3 + Y * (Q4 + Y * (Q5 + Y * (Q6 + Y * (Q7 + Y * (Q8 + Y * Q9)))))));
        return (AX * BX);
    }
}

inline double symmetric_kaiser(double x, uint16_t nint, double Alpha)
{
    double N = (double)nint;
    x += N * 0.5;

    if (x > N)
        x = N;
    if (x < 0.0)
        x = 0.0;
    // x = std::min((double)x, (double)N);
    // x = std::max(x, 0.0);
    double a = (2.0 * x / N - 1.0);
    return BESSI0(M_PI * Alpha * sqrt(1.0 - a * a)) / BESSI0(M_PI * Alpha);
}

}

#endif // SURGE_SPECIALFUNCTIONS_H
