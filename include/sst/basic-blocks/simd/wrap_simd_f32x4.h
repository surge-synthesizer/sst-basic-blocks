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

#ifndef INCLUDE_SST_BASIC_BLOCKS_SIMD_WRAP_SIMD_F32X4_H
#define INCLUDE_SST_BASIC_BLOCKS_SIMD_WRAP_SIMD_F32X4_H

#include "setup.h"
#include <type_traits>

namespace sst::basic_blocks::simd
{
/**
 * This is a simple and lightly tested class which we added
 * to break a dependency on juce::SIMDRegister<float> we had
 * in surge. If you are going to use it extensivelly some more
 * profiling and testing would be appropriate.
 *
 * See a basic smoke test in simd_tests.cpp
 */
struct F32x4
{
    using value_type = float;

    SIMD_M128 val;

    F32x4() { val = SIMD_MM(setzero_ps)(); }

    F32x4(SIMD_M128 v) { val = v; }

    template <typename T>
        requires std::is_convertible_v<T, float>
    F32x4(const T f)
    {
        val = SIMD_MM(set1_ps)((float)f);
    }

    F32x4(const float f[4]) { val = SIMD_MM(load_ps)(f); }

    static F32x4 fromRawArray(const float f[4]) { return F32x4(f); }

    void copyToRawArray(float f[4]) const { SIMD_MM(store_ps)(f, val); }

    template <typename T>
        requires std::is_convertible_v<T, float>
    F32x4 &operator=(const T f)
    {
        val = SIMD_MM(set1_ps)((float)f);
        return *this;
    }

    F32x4 &operator+=(const F32x4 &b)
    {
        val = SIMD_MM(add_ps)(val, b.val);
        return *this;
    }

    F32x4 &operator-=(const F32x4 &b)
    {
        val = SIMD_MM(sub_ps)(val, b.val);
        return *this;
    }

    F32x4 &operator*=(const F32x4 &b)
    {
        val = SIMD_MM(mul_ps)(val, b.val);
        return *this;
    }

    F32x4 &operator/=(const F32x4 &b)
    {
        val = SIMD_MM(div_ps)(val, b.val);
        return *this;
    }
};

inline F32x4 operator+(const F32x4 &a, const F32x4 &b)
{
    return F32x4(SIMD_MM(add_ps)(a.val, b.val));
}

inline F32x4 operator-(const F32x4 &a, const F32x4 &b)
{
    return F32x4(SIMD_MM(sub_ps)(a.val, b.val));
}

inline F32x4 operator*(const F32x4 &a, const F32x4 &b)
{
    return F32x4(SIMD_MM(mul_ps)(a.val, b.val));
}

inline F32x4 operator/(const F32x4 &a, const F32x4 &b)
{
    return F32x4(SIMD_MM(div_ps)(a.val, b.val));
}
} // namespace sst::basic_blocks::simd
#endif // SURGE_WRAP_SIMD_F32X4_H
