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

#ifndef SST_BASICBLOCKS_DSP_CLIPPERS_H
#define SST_BASICBLOCKS_DSP_CLIPPERS_H

namespace sst::basic_blocks::dsp
{

/**
 * y = x - (4/27)*x^3,  x in [-1.5 .. 1.5], +/-1 otherwise
 */
inline __m128 softclip_ps(__m128 in)
{
    const __m128 a = _mm_set1_ps(-4.f / 27.f);

    const __m128 x_min = _mm_set1_ps(-1.5f);
    const __m128 x_max = _mm_set1_ps(1.5f);

    __m128 x = _mm_max_ps(_mm_min_ps(in, x_max), x_min);
    __m128 xx = _mm_mul_ps(x, x);
    __m128 t = _mm_mul_ps(x, a);
    t = _mm_mul_ps(t, xx);
    t = _mm_add_ps(t, x);

    return t;
}


/**
 * y = x - (4/27/8^3)*x^3,  x in [-12 .. 12], +/-12 otherwise
 */
inline __m128 softclip8_ps(__m128 in)
{
    /*
     * This constant is - 4/27 / 8^3 so it "scales" the
     * coefficient but if you look at the plot I don't think
     * that's quite right - it doesn't have the right smoothness.
     * But this is only used in one spot - in LPMOOGquad - so
     * we will just leave it for now
     */
    const __m128 a = _mm_set1_ps(-0.00028935185185f);

    const __m128 x_min = _mm_set1_ps(-12.f);
    const __m128 x_max = _mm_set1_ps(12.f);

    __m128 x = _mm_max_ps(_mm_min_ps(in, x_max), x_min);
    __m128 xx = _mm_mul_ps(x, x);
    __m128 t = _mm_mul_ps(x, a);
    t = _mm_mul_ps(t, xx);
    t = _mm_add_ps(t, x);
    return t;
}

inline __m128 tanh7_ps(__m128 v)
{
    const __m128 upper_bound = _mm_set1_ps(1.139f);
    const __m128 lower_bound = _mm_set1_ps(-1.139f);
    auto x = _mm_max_ps(v, lower_bound);
    x = _mm_min_ps(x, upper_bound);

    const __m128 a = _mm_set1_ps(-1.f / 3.f);
    const __m128 b = _mm_set1_ps(2.f / 15.f);
    const __m128 c = _mm_set1_ps(-17.f / 315.f);
    const __m128 one = _mm_set1_ps(1.f);
    __m128 xx = _mm_mul_ps(x, x);
    __m128 y = _mm_add_ps(one, _mm_mul_ps(a, xx));
    __m128 x4 = _mm_mul_ps(xx, xx);
    y = _mm_add_ps(y, _mm_mul_ps(b, x4));
    x4 = _mm_mul_ps(x4, xx);
    y = _mm_add_ps(y, _mm_mul_ps(c, x4));
    return _mm_mul_ps(y, x);
}

template <size_t blockSize>
void softclip_block(float *__restrict x)
{
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        _mm_store_ps(x + i, softclip_ps(_mm_load_ps(x + i)));
    }
}

template <size_t blockSize>
void tanh7_block(float *__restrict x)
{
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        _mm_store_ps(x + i, tanh7_ps(_mm_load_ps(x + i)));
    }
}

template<size_t blockSize>
void hardclip_block(float *x)
{
    static_assert(!(blockSize & (blockSize - 1)) && blockSize >= 4);
    const __m128 x_min = _mm_set1_ps(-1.0f);
    const __m128 x_max = _mm_set1_ps(1.0f);
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        _mm_store_ps(x + i, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i), x_max), x_min));
    }
}


template<size_t blockSize>
void hardclip_block8(float *x)
{
    static_assert(!(blockSize & (blockSize - 1)) && blockSize >= 4);
    const __m128 x_min = _mm_set1_ps(-8.0f);
    const __m128 x_max = _mm_set1_ps(8.0f);
    for (unsigned int i = 0; i < blockSize; i += 4)
    {
        _mm_store_ps(x + i, _mm_max_ps(_mm_min_ps(_mm_load_ps(x + i), x_max), x_min));
    }
}
}

#endif // SURGE_SHAPERS_H
