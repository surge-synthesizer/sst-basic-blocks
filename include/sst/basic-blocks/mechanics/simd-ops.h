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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MECHANICS_SIMD_OPS_H
#define INCLUDE_SST_BASIC_BLOCKS_MECHANICS_SIMD_OPS_H

#include "sst/basic-blocks/simd/setup.h"

namespace sst::basic_blocks::mechanics
{
inline SIMD_M128 sum_ps_to_ss(SIMD_M128 x)
{
    // FIXME: With SSE 3 this can be a dual hadd
    auto a = SIMD_MM(add_ps)(x, SIMD_MM(movehl_ps)(x, x));
    return SIMD_MM(add_ss)(a, SIMD_MM(shuffle_ps)(a, a, SIMD_MM_SHUFFLE(0, 0, 0, 1)));
}

inline float sum_ps_to_float(SIMD_M128 x)
{
    // MSVC can ambiguously resolve this while it still lives in surge vt_dsp alas
    auto r = sst::basic_blocks::mechanics::sum_ps_to_ss(x);
    float f;
    SIMD_MM(store_ss)(&f, r);
    return f;
}

namespace detail
{
inline float i2f_binary_cast(int i)
{
    float *f = (float *)&i;
    return *f;
}
} // namespace detail

const auto m128_mask_signbit = SIMD_MM(set1_ps)(detail::i2f_binary_cast(0x80000000));
const auto m128_mask_absval = SIMD_MM(set1_ps)(detail::i2f_binary_cast(0x7fffffff));

inline SIMD_M128 abs_ps(SIMD_M128 x) { return SIMD_MM(and_ps)(x, m128_mask_absval); }

inline float rcp(float x)
{
    SIMD_MM(store_ss)(&x, SIMD_MM(rcp_ss)(SIMD_MM(load_ss)(&x)));
    return x;
}

} // namespace sst::basic_blocks::mechanics

#endif // SHORTCIRCUIT_SIMD_OPS_H
