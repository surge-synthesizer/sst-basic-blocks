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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MECHANICS_BLOCK_OPS_H
#define INCLUDE_SST_BASIC_BLOCKS_MECHANICS_BLOCK_OPS_H

#include <algorithm>
#include <cmath>
#include <cstring>

#include "simd-ops.h"

namespace sst::basic_blocks::mechanics
{

template <size_t blocksize> inline void clear_block(float *__restrict f)
{
    memset(f, 0, blocksize * sizeof(float));
}
// I checked these with clang / godbolt and they output the same SIMD I would code by hand
// once you add the __restrict keyword
template <size_t blocksize>
inline void accumulate_from_to(const float *__restrict src, float *__restrict dst)
{
    for (auto i = 0U; i < blocksize; ++i)
        dst[i] += src[i];
}

template <size_t blocksize>
inline void scale_accumulate_from_to(const float *__restrict src, float scale,
                                     float *__restrict dst)
{
    for (auto i = 0U; i < blocksize; ++i)
        dst[i] += src[i] * scale;
}

template <size_t blocksize>
inline void scale_accumulate_from_to(const float *__restrict srcL, float *__restrict srcR,
                                     float scale, float *__restrict dstL, float *__restrict dstR)
{
    for (auto i = 0U; i < blocksize; ++i)
    {
        dstR[i] += srcR[i] * scale;
        dstL[i] += srcL[i] * scale;
    }
}

template <size_t blocksize>
inline void copy_from_to(const float *__restrict src, float *__restrict dst)
{
    for (auto i = 0U; i < blocksize; ++i)
        dst[i] = src[i];
}

template <size_t blocksize>
inline void add_block(const float *__restrict src1, const float *__restrict src2,
                      float *__restrict dst)
{
    for (auto i = 0U; i < blocksize; ++i)
    {
        dst[i] = src1[i] + src2[i];
    }
}

template <size_t blocksize>
inline void add_block(float *__restrict srcdst, const float *__restrict src2)
{
    for (auto i = 0U; i < blocksize; ++i)
    {
        srcdst[i] = srcdst[i] + src2[i];
    }
}

template <size_t blockSize>
inline void mul_block(const float *__restrict const src1, const float *const src2,
                      float *__restrict dst)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        dst[i] = src1[i] * src2[i];
    }
}

template <size_t blockSize>
inline void mul_block(float *__restrict src1, float scalar, float *__restrict dst)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        dst[i] = src1[i] * scalar;
    }
}

template <size_t blockSize> inline void mul_block(float *__restrict srcDst, float *__restrict by)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        srcDst[i] = srcDst[i] * by[i];
    }
}

template <size_t blockSize> inline void mul_block(float *__restrict srcDst, float by)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        srcDst[i] = srcDst[i] * by;
    }
}

template <size_t blockSize>
inline void scale_by(const float *__restrict scale, float *__restrict target)
{
    for (auto i = 0U; i < blockSize; ++i)
        target[i] *= scale[i];
}

template <size_t blockSize>
inline void scale_by(const float *__restrict scale, float *__restrict targetL,
                     float *__restrict targetR)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        targetL[i] *= scale[i];
        targetR[i] *= scale[i];
    }
}

template <size_t blockSize> inline void scale_by(const float scale, float *__restrict target)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        target[i] *= scale;
    }
}

template <size_t blockSize>
inline void scale_by(const float scale, float *__restrict targetL, float *__restrict targetR)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        targetL[i] *= scale;
        targetR[i] *= scale;
    }
}

template <size_t blockSize> inline float blockAbsMax(const float *__restrict d)
{
    // Note there used to be a SIMD version of this before hash fc9ab6258c93e
    // which was slower with modern compilers
    auto r = 0.f;
    for (auto i = 0U; i < blockSize; ++i)
        r = std::max(r, std::fabs(d[i]));
    return r;
}
} // namespace sst::basic_blocks::mechanics

#endif // SHORTCIRCUIT_BLOCK_OPS_H
