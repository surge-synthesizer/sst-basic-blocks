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
#ifndef SST_BASIC_BLOCKS_MECHANICS_SIMD_OPS_H
#define SST_BASIC_BLOCKS_MECHANICS_SIMD_OPS_H

namespace sst::basic_blocks::mechanics
{
inline __m128 sum_ps_to_ss(__m128 x)
{
    // FIXME: With SSE 3 this can be a dual hadd
    __m128 a = _mm_add_ps(x, _mm_movehl_ps(x, x));
    return _mm_add_ss(a, _mm_shuffle_ps(a, a, _MM_SHUFFLE(0, 0, 0, 1)));
}

inline float sum_ps_to_float(__m128 x)
{
    // MSVC can ambiguously resolve this while it still lives in surge vt_dsp alas
    __m128 r = sst::basic_blocks::mechanics::sum_ps_to_ss(x);
    float f;
    _mm_store_ss(&f, r);
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

const __m128 m128_mask_signbit = _mm_set1_ps(detail::i2f_binary_cast(0x80000000));
const __m128 m128_mask_absval = _mm_set1_ps(detail::i2f_binary_cast(0x7fffffff));

inline __m128 abs_ps(__m128 x) { return _mm_and_ps(x, m128_mask_absval); }

inline float rcp(float x)
{
    _mm_store_ss(&x, _mm_rcp_ss(_mm_load_ss(&x)));
    return x;
}

} // namespace sst::basic_blocks::mechanics

#endif // SHORTCIRCUIT_SIMD_OPS_H
