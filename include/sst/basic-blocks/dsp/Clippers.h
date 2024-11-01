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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_CLIPPERS_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_CLIPPERS_H

#include "sst/basic-blocks/simd/setup.h"

namespace sst::basic_blocks::dsp
{

/**
 * y = x - (4/27)*x^3,  x in [-1.5 .. 1.5], +/-1 otherwise
 */
inline SIMD_M128 softclip_ps(SIMD_M128 in)
{
    const auto a = SIMD_MM(set1_ps)(-4.f / 27.f);

    const auto x_min = SIMD_MM(set1_ps)(-1.5f);
    const auto x_max = SIMD_MM(set1_ps)(1.5f);

    auto x = SIMD_MM(max_ps)(SIMD_MM(min_ps)(in, x_max), x_min);
    auto xx = SIMD_MM(mul_ps)(x, x);
    auto t = SIMD_MM(mul_ps)(x, a);
    t = SIMD_MM(mul_ps)(t, xx);
    t = SIMD_MM(add_ps)(t, x);

    return t;
}

/**
 * y = x - (4/27/8^3)*x^3,  x in [-12 .. 12], +/-12 otherwise
 */
inline SIMD_M128 softclip8_ps(SIMD_M128 in)
{
    /*
     * This constant is - 4/27 / 8^3 so it "scales" the
     * coefficient but if you look at the plot I don't think
     * that's quite right - it doesn't have the right smoothness.
     * But this is only used in one spot - in LPMOOGquad - so
     * we will just leave it for now
     */
    const auto a = SIMD_MM(set1_ps)(-0.00028935185185f);

    const auto x_min = SIMD_MM(set1_ps)(-12.f);
    const auto x_max = SIMD_MM(set1_ps)(12.f);

    auto x = SIMD_MM(max_ps)(SIMD_MM(min_ps)(in, x_max), x_min);
    auto xx = SIMD_MM(mul_ps)(x, x);
    auto t = SIMD_MM(mul_ps)(x, a);
    t = SIMD_MM(mul_ps)(t, xx);
    t = SIMD_MM(add_ps)(t, x);
    return t;
}

inline SIMD_M128 tanh7_ps(SIMD_M128 v)
{
    const auto upper_bound = SIMD_MM(set1_ps)(1.139f);
    const auto lower_bound = SIMD_MM(set1_ps)(-1.139f);
    auto x = SIMD_MM(max_ps)(v, lower_bound);
    x = SIMD_MM(min_ps)(x, upper_bound);

    const auto a = SIMD_MM(set1_ps)(-1.f / 3.f);
    const auto b = SIMD_MM(set1_ps)(2.f / 15.f);
    const auto c = SIMD_MM(set1_ps)(-17.f / 315.f);
    const auto one = SIMD_MM(set1_ps)(1.f);
    auto xx = SIMD_MM(mul_ps)(x, x);
    auto y = SIMD_MM(add_ps)(one, SIMD_MM(mul_ps)(a, xx));
    auto x4 = SIMD_MM(mul_ps)(xx, xx);
    y = SIMD_MM(add_ps)(y, SIMD_MM(mul_ps)(b, x4));
    x4 = SIMD_MM(mul_ps)(x4, xx);
    y = SIMD_MM(add_ps)(y, SIMD_MM(mul_ps)(c, x4));
    return SIMD_MM(mul_ps)(y, x);
}

template <size_t blockSize> void softclip_block(float *__restrict x)
{
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        SIMD_MM(store_ps)(x + i, softclip_ps(SIMD_MM(load_ps)(x + i)));
    }
}

template <size_t blockSize> void tanh7_block(float *__restrict x)
{
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        SIMD_MM(store_ps)(x + i, tanh7_ps(SIMD_MM(load_ps)(x + i)));
    }
}

template <size_t blockSize> void hardclip_block(float *x)
{
    static_assert(!(blockSize & (blockSize - 1)) && blockSize >= 4);
    const auto x_min = SIMD_MM(set1_ps)(-1.0f);
    const auto x_max = SIMD_MM(set1_ps)(1.0f);
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        SIMD_MM(store_ps)
        (x + i, SIMD_MM(max_ps)(SIMD_MM(min_ps)(SIMD_MM(load_ps)(x + i), x_max), x_min));
    }
}

template <size_t blockSize> void hardclip_block8(float *x)
{
    static_assert(!(blockSize & (blockSize - 1)) && blockSize >= 4);
    const auto x_min = SIMD_MM(set1_ps)(-8.0f);
    const auto x_max = SIMD_MM(set1_ps)(8.0f);
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        SIMD_MM(store_ps)
        (x + i, SIMD_MM(max_ps)(SIMD_MM(min_ps)(SIMD_MM(load_ps)(x + i), x_max), x_min));
    }
}
} // namespace sst::basic_blocks::dsp

#endif // SURGE_SHAPERS_H
