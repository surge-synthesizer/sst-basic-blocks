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

#ifndef SST_BASIC_BLOCKS_DSP_SINC_DELAY_LINE_H
#define SST_BASIC_BLOCKS_DSP_SINC_DELAY_LINE_H

#include "sst/basic-blocks/mechanics/simd-ops.h"
#include "sst/basic-blocks/tables/SincTableProvider.h"

namespace sst::basic_blocks::dsp
{

/*
 * This is a template class which encapsulates the SSE based SINC
 * interpolation in COMBquad_SSE2,just made available for other uses
 */
template <int COMB_SIZE> // power of two
struct SSESincDelayLine
{
    static_assert(! (COMB_SIZE & (COMB_SIZE-1))); // make sure we are a power of 2
    static constexpr int comb_size = COMB_SIZE;

    using stp=tables::SurgeSincTableProvider;

    float buffer alignas(16)[COMB_SIZE + stp::FIRipol_N];
    int wp = 0;

    const float *sinctable{nullptr}; // a pointer copy of the storage member
    SSESincDelayLine(const float *st) : sinctable(st) { clear(); }

    /**
     * If you ahve a long lived instance of a SurgeSincTableProvider you can initialize with that
     * but please make sure that table has lifetime longer than this interpolator, since we take the
     * pointer address of its table
     */
    SSESincDelayLine(const tables::SurgeSincTableProvider &st) : sinctable(st.sinctable) { clear(); }

    inline void write(float f)
    {
        buffer[wp] = f;
        buffer[wp + (wp < stp::FIRipol_N) * COMB_SIZE] = f;
        wp = (wp + 1) & (COMB_SIZE - 1);
    }

    inline float read(float delay)
    {
        auto iDelay = (int)delay;
        auto fracDelay = delay - iDelay;
        auto sincTableOffset = (int)((1 - fracDelay) * stp::FIRipol_M) * stp::FIRipol_N * 2;

        // So basically we interpolate around stp::FIRipol_N (the 12 sample sinc)
        // remembering that stp::FIRoffset is the offset to center your table at
        // a point ( it is stp::FIRipol_N >> 1)
        int readPtr = (wp - iDelay - (stp::FIRipol_N >> 1)) & (COMB_SIZE - 1);

        // And so now do what we do in COMBSSE2Quad
        __m128 a = _mm_loadu_ps(&buffer[readPtr]);
        __m128 b = _mm_loadu_ps(&sinctable[sincTableOffset]);
        __m128 o = _mm_mul_ps(a, b);

        a = _mm_loadu_ps(&buffer[readPtr + 4]);
        b = _mm_loadu_ps(&sinctable[sincTableOffset + 4]);
        o = _mm_add_ps(o, _mm_mul_ps(a, b));

        a = _mm_loadu_ps(&buffer[readPtr + 8]);
        b = _mm_loadu_ps(&sinctable[sincTableOffset + 8]);
        o = _mm_add_ps(o, _mm_mul_ps(a, b));

        float res;
        _mm_store_ss(&res, sst::basic_blocks::mechanics::sum_ps_to_ss(o));

        return res;
    }

    inline float readLinear(float delay)
    {
        auto iDelay = (int)delay;
        auto frac = delay - iDelay;
        int RP = (wp - iDelay) & (COMB_SIZE - 1);
        int RPP = RP == 0 ? COMB_SIZE - 1 : RP - 1;
        return buffer[RP] * (1 - frac) + buffer[RPP] * frac;
    }

    inline float readZOH(float delay)
    {
        auto iDelay = (int)delay;
        int RP = (wp - iDelay) & (COMB_SIZE - 1);
        int RPP = RP == 0 ? COMB_SIZE - 1 : RP - 1;
        return buffer[RPP];
    }

    inline void clear()
    {
        memset((void *)buffer, 0, (COMB_SIZE + stp::FIRipol_N) * sizeof(float));
        wp = 0;
    }
};
}
#endif // SURGE_SSESINCDELAYLINE_H
