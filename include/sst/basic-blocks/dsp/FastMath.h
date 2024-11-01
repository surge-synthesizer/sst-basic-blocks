/*
 * sst-basic-blocks - an open source library of core audio utilities
 * built by Surge Synth Team.
 *
 * Provides a collection of tools useful on the audio thread for blocks,
 * modulation, etc... or useful for adapting code to multiple environments.
 *
 * Copyright 2023, various authors, as described in the GitHub
 * transaction log. Parts of this code are derived from similar
 * functions original in Surge or ShortCircuit.
 *
 * sst-basic-blocks is released under the GNU General Public Licence v3
 * or later (GPL-3.0-or-later). The license is found in the "LICENSE"
 * file in the root of this repository, or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html.
 *
 * A very small number of explicitly chosen header files can also be
 * used in an MIT/BSD context. Please see the README.md file in this
 * repo or the comments in the individual files. Only headers with an
 * explicit mention that they are dual licensed may be copied and reused
 * outside the GPL3 terms.
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_FASTMATH_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_FASTMATH_H

#include <cmath>
#include "sst/basic-blocks/simd/setup.h"

/*
** Fast Math Approximations to various Functions
**
** Documentation on each one
*/

/*
 * Many of these are directly copied from JUCE6
 * modules/juce_dsp/mathos/juce_FastMathApproximations.h
 *
 * Since JUCE6 is GPL3 and Surge is GPL3 that copy is fine, but I did want to explicitly
 * acknowledge it
 */

namespace sst::basic_blocks::dsp
{

// JUCE6 Pade approximation of sin valid from -PI to PI with max error of 1e-5 and average error of
// 5e-7
inline float fastsin(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = -x * (-(float)11511339840 +
                           x2 * ((float)1640635920 + x2 * (-(float)52785432 + x2 * (float)479249)));
    auto denominator =
        (float)11511339840 + x2 * ((float)277920720 + x2 * ((float)3177720 + x2 * (float)18361));
    return numerator / denominator;
}

inline SIMD_M128 fastsinSSE(SIMD_M128 x) noexcept
{
#define M(a, b) SIMD_MM(mul_ps)(a, b)
#define A(a, b) SIMD_MM(add_ps)(a, b)
#define S(a, b) SIMD_MM(sub_ps)(a, b)
#define F(a) SIMD_MM(set_ps1)(a)
#define C(x) auto m##x = F((float)x)

    /*
    auto numerator = -x * (-(float)11511339840 +
                           x2 * ((float)1640635920 + x2 * (-(float)52785432 + x2 * (float)479249)));
    auto denominator =
        (float)11511339840 + x2 * ((float)277920720 + x2 * ((float)3177720 + x2 * (float)18361));
        */
    C(11511339840);
    C(1640635920);
    C(52785432);
    C(479249);
    C(277920720);
    C(3177720);
    C(18361);
    auto mnegone = F(-1);

    auto x2 = M(x, x);

    auto num = M(mnegone,
                 M(x, S(M(x2, A(m1640635920, M(x2, S(M(x2, m479249), m52785432)))), m11511339840)));
    auto den = A(m11511339840, M(x2, A(m277920720, M(x2, A(m3177720, M(x2, m18361))))));

#undef C
#undef M
#undef A
#undef S
#undef F
    return SIMD_MM(div_ps)(num, den);
}

// JUCE6 Pade approximation of cos valid from -PI to PI with max error of 1e-5 and average error of
// 5e-7
inline float fastcos(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = -(-(float)39251520 + x2 * ((float)18471600 + x2 * (-1075032 + 14615 * x2)));
    auto denominator = (float)39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
    return numerator / denominator;
}

inline SIMD_M128 fastcosSSE(SIMD_M128 x) noexcept
{
#define M(a, b) SIMD_MM(mul_ps)(a, b)
#define A(a, b) SIMD_MM(add_ps)(a, b)
#define S(a, b) SIMD_MM(sub_ps)(a, b)
#define F(a) SIMD_MM(set_ps1)(a)
#define C(x) auto m##x = F((float)x)

    // auto x2 = x * x;
    auto x2 = M(x, x);

    C(39251520);
    C(18471600);
    C(1075032);
    C(14615);
    C(1154160);
    C(16632);
    C(127);

    // auto numerator = -(-(float)39251520 + x2 * ((float)18471600 + x2 * (-1075032 + 14615 * x2)));
    auto num = S(m39251520, M(x2, A(m18471600, M(x2, S(M(m14615, x2), m1075032)))));

    // auto denominator = (float)39251520 + x2 * (1154160 + x2 * (16632 + x2 * 127));
    auto den = A(m39251520, M(x2, A(m1154160, M(x2, A(m16632, M(x2, m127))))));
#undef C
#undef M
#undef A
#undef S
#undef F
    return SIMD_MM(div_ps)(num, den);
}

/*
** Push x into -PI, PI periodically. There is probably a faster way to do this
*/
inline float clampToPiRange(float x)
{
    if (x <= M_PI && x >= -M_PI)
        return x;
    float y = x + M_PI; // so now I am 0,2PI

    // float p = fmod( y, 2.0 * M_PI );

    constexpr float oo2p = 1.0 / (2.0 * M_PI);
    float p = y - 2.0 * M_PI * (int)(y * oo2p);

    if (p < 0)
        p += 2.0 * M_PI;
    return p - M_PI;
}

inline SIMD_M128 clampToPiRangeSSE(SIMD_M128 x)
{
    const auto mpi = SIMD_MM(set1_ps)(M_PI);
    const auto m2pi = SIMD_MM(set1_ps)(2.0 * M_PI);
    const auto oo2p = SIMD_MM(set1_ps)(1.0 / (2.0 * M_PI));
    const auto mz = SIMD_MM(setzero_ps)();

    auto y = SIMD_MM(add_ps)(x, mpi);
    auto yip = SIMD_MM(cvtepi32_ps)(SIMD_MM(cvttps_epi32)(SIMD_MM(mul_ps)(y, oo2p)));
    auto p = SIMD_MM(sub_ps)(y, SIMD_MM(mul_ps)(m2pi, yip));
    auto off = SIMD_MM(and_ps)(SIMD_MM(cmplt_ps)(p, mz), m2pi);
    p = SIMD_MM(add_ps)(p, off);

    return SIMD_MM(sub_ps)(p, mpi);
}

/*
** Valid in range -5, 5
*/
inline float fasttanh(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = x * (135135 + x2 * (17325 + x2 * (378 + x2)));
    auto denominator = 135135 + x2 * (62370 + x2 * (3150 + 28 * x2));
    return numerator / denominator;
}

// Valid in range (-PI/2, PI/2)
inline float fasttan(float x) noexcept
{
    auto x2 = x * x;
    auto numerator = x * (-135135 + x2 * (17325 + x2 * (-378 + x2)));
    auto denominator = -135135 + x2 * (62370 + x2 * (-3150 + 28 * x2));
    return numerator / denominator;
}

inline SIMD_M128 fasttanhSSE(SIMD_M128 x)
{
    const auto m135135 = SIMD_MM(set_ps1)(135135), m17325 = SIMD_MM(set_ps1)(17325),
               m378 = SIMD_MM(set_ps1)(378), m62370 = SIMD_MM(set_ps1)(62370),
               m3150 = SIMD_MM(set_ps1)(3150), m28 = SIMD_MM(set_ps1)(28);

#define M(a, b) SIMD_MM(mul_ps)(a, b)
#define A(a, b) SIMD_MM(add_ps)(a, b)

    auto x2 = M(x, x);
    auto num = M(x, A(m135135, M(x2, A(m17325, M(x2, A(m378, x2))))));
    auto den = A(m135135, M(x2, A(m62370, M(x2, A(m3150, M(m28, x2))))));

#undef M
#undef A

    return SIMD_MM(div_ps)(num, den);
}

inline SIMD_M128 fasttanhSSEclamped(SIMD_M128 x)
{
    auto xc = SIMD_MM(min_ps)(SIMD_MM(set_ps1)(5), SIMD_MM(max_ps)(SIMD_MM(set_ps1)(-5), x));
    return fasttanhSSE(xc);
}

/*
** Valid in range -6, 4
*/
inline float fastexp(float x) noexcept
{
    auto numerator = 1680 + x * (840 + x * (180 + x * (20 + x)));
    auto denominator = 1680 + x * (-840 + x * (180 + x * (-20 + x)));
    return numerator / denominator;
}

inline SIMD_M128 fastexpSSE(SIMD_M128 x) noexcept
{
#define M(a, b) SIMD_MM(mul_ps)(a, b)
#define A(a, b) SIMD_MM(add_ps)(a, b)
#define F(a) SIMD_MM(set_ps1)(a)

    const auto m1680 = F(1680), m840 = F(840), mneg840 = F(-840), m180 = F(180), m20 = F(20),
               mneg20 = F(-20);

    auto num = A(m1680, M(x, A(m840, M(x, A(m180, M(x, A(m20, x)))))));
    auto den = A(m1680, M(x, A(mneg840, M(x, A(m180, M(x, A(mneg20, x)))))));

    return SIMD_MM(div_ps)(num, den);

#undef M
#undef A
#undef F
}

} // namespace sst::basic_blocks::dsp
#endif
