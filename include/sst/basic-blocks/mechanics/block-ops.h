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

#ifndef SST_BASIC_BLOCKS_MECHANICS_BLOCK_OPS_H
#define SST_BASIC_BLOCKS_MECHANICS_BLOCK_OPS_H

#include <algorithm>
#include <cmath>

#include "simd-ops.h"

namespace sst::basic_blocks::mechanics
{

template<size_t blocksize>
inline void clear_block(float *__restrict f)
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
inline void copy_from_to(const float *__restrict src, float *__restrict dst)
{
    for (auto i = 0U; i < blocksize; ++i)
        dst[i] = src[i];
}

template<size_t blocksize>
inline void add_block(const float *__restrict src1, const float *__restrict src2, float *__restrict dst)
{
    for (auto i = 0U; i < blocksize; ++i)
    {
        dst[i] = src1[i] + src2[i];
    }
}

template<size_t blockSize>
inline void mul_block(float *__restrict src1, float *src2, float *__restrict dst)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        dst[i] = src1[i] * src2[i];
    }
}

template<size_t blockSize>
inline void mul_block(float *__restrict src1, float scalar, float *__restrict dst)
{
    for (auto i = 0U; i < blockSize; ++i)
    {
        dst[i] = src1[i] * scalar;
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

template <size_t blockSize>
inline float blockAbsMax(const float *__restrict d)
{
#if 0
    // Note there used to be a SIMD version of this
    auto r = 0.f;
    for (auto i=0U; i<blockSize; ++i)
        r = std::max(r, std::fabs(d[i]));
#else
    __m128 mx = _mm_setzero_ps();

    for (unsigned int i = 0; i < blockSize >> 2; i ++)
    {
        mx = _mm_max_ps(mx, _mm_and_ps(((__m128 *)d)[i], m128_mask_absval));
    }

    float r alignas(16)[4];
    _mm_store_ps(r, mx);

    return std::max({r[0], r[1], r[2], r[3]});
#endif

}
} // namespace sst::basic_blocks::mechanics

#endif // SHORTCIRCUIT_BLOCK_OPS_H
