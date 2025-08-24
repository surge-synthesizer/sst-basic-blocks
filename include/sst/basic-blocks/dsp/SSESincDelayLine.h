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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_SSESINCDELAYLINE_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_SSESINCDELAYLINE_H

#include "sst/basic-blocks/simd/setup.h"
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
    static_assert(!(COMB_SIZE & (COMB_SIZE - 1))); // make sure we are a power of 2
    static constexpr int comb_size = COMB_SIZE;

    using stp = tables::SurgeSincTableProvider;

    float buffer alignas(16)[COMB_SIZE + stp::FIRipol_N];
    int wp = 0;

    const float *sinctable{nullptr}; // a pointer copy of the storage member
    SSESincDelayLine(const float *st) : sinctable(st) { clear(); }

    /**
     * If you ahve a long lived instance of a SurgeSincTableProvider you can initialize with that
     * but please make sure that table has lifetime longer than this interpolator, since we take the
     * pointer address of its table
     */
    SSESincDelayLine(const tables::SurgeSincTableProvider &st) : sinctable(st.sinctable)
    {
        clear();
    }

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
        auto a = SIMD_MM(loadu_ps)(&buffer[readPtr]);
        auto b = SIMD_MM(loadu_ps)(&sinctable[sincTableOffset]);
        auto o = SIMD_MM(mul_ps)(a, b);

        a = SIMD_MM(loadu_ps)(&buffer[readPtr + 4]);
        b = SIMD_MM(loadu_ps)(&sinctable[sincTableOffset + 4]);
        o = SIMD_MM(add_ps)(o, SIMD_MM(mul_ps)(a, b));

        a = SIMD_MM(loadu_ps)(&buffer[readPtr + 8]);
        b = SIMD_MM(loadu_ps)(&sinctable[sincTableOffset + 8]);
        o = SIMD_MM(add_ps)(o, SIMD_MM(mul_ps)(a, b));

        float res;
        SIMD_MM(store_ss)(&res, sst::basic_blocks::mechanics::sum_ps_to_ss(o));

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

/*
 * This is a class which encapsulates the SSE based SINC
 * interpolation without maintaining a circular buffer, and requiring
 * that you provide an external buffer. That buffer has to be padded by
 * FIRipol_N (==12) samples on either side. The indexing is at sample 0.
 * So you provide a buffer with
 *
 * 012345678901234     ...... n012345678901
 *             ^               ^ pad zeroes at end
 *             ^ your data and point 0 start here
 *
 * By default your buffer is a single channel but you can provide a
 * buffer with interleave n for n-channel data and set the stride template
 * param
 */

template <uint32_t stride> struct SSESincInterpolater
{
    using stp = tables::SurgeSincTableProvider;

    const float *data{nullptr};
    size_t frames{1}, offset{stp::FIRipol_N};
    const float *sinctable{nullptr}; // a pointer copy of the storage member
    SSESincInterpolater(const float *st, float *d, size_t frames)
        : sinctable(st), frames(frames), data(d)
    {
    }

    /**
     * If you have a long lived instance of a SurgeSincTableProvider you can initialize with that
     * but please make sure that table has lifetime longer than this interpolator, since we take the
     * pointer address of its table
     */
    SSESincInterpolater(const tables::SurgeSincTableProvider &st, float *d, size_t frames)
        : sinctable(st.sinctable), data(d), frames(frames)
    {
    }

    inline SIMD_M128 pop4(size_t posn)
    {
        if constexpr (stride == 1)
        {
            return SIMD_MM(loadu_ps)(&data[posn]);
        }
        else
        {
            float tmp alignas(16)[4];
            tmp[0] = data[posn];
            tmp[1] = data[posn + stride];
            tmp[2] = data[posn + 2 * stride];
            tmp[3] = data[posn + 3 * stride];
            return SIMD_MM(load_ps)(tmp);
        }
    }

    inline float read(float posn)
    {
        auto iPosn = (size_t)posn;
        auto fracPosn = posn - iPosn;
        // iPosn += (fracPosn > 0) * 1;

        auto sincTableOffset = (int)(fracPosn * stp::FIRipol_M) * stp::FIRipol_N * 2;

        // So basically we interpolate around stp::FIRipol_N (the 12 sample sinc)
        // remembering that stp::FIRoffset is the offset to center your table at
        // a point ( it is stp::FIRipol_N >> 1)
        int readPtr = stride * (iPosn + (offset + 1) - (stp::FIRipol_N >> 1));
        // std::cout << "p=" << posn << " ip=" << iPosn << " fp=" << fracPosn << " rp=" << readPtr
        // << " sto=" << sincTableOffset << std::endl;

        // And so now do what we do in COMBSSE2Quad
        auto a = pop4(readPtr);
        auto b = SIMD_MM(loadu_ps)(&sinctable[sincTableOffset]);
        auto o = SIMD_MM(mul_ps)(a, b);

        a = pop4(readPtr + 4 * stride);
        b = SIMD_MM(loadu_ps)(&sinctable[sincTableOffset + 4]);
        o = SIMD_MM(add_ps)(o, SIMD_MM(mul_ps)(a, b));

        a = pop4(readPtr + 8 * stride);
        b = SIMD_MM(loadu_ps)(&sinctable[sincTableOffset + 8]);
        o = SIMD_MM(add_ps)(o, SIMD_MM(mul_ps)(a, b));

        float res;
        SIMD_MM(store_ss)(&res, sst::basic_blocks::mechanics::sum_ps_to_ss(o));

        return res;
    }

    inline float readLinear(float posn)
    {
        auto iPosn = (int)posn;
        auto frac = posn - iPosn;
        int RP = iPosn + offset;
        int RPP = iPosn + offset + 1;
        return data[stride * RP] * (1 - frac) + data[stride * RPP] * frac;
    }

    inline float readZOH(float posn)
    {
        auto iDelay = (int)posn;
        int RP = iDelay + offset;
        return data[stride * RP];
    }
};

} // namespace sst::basic_blocks::dsp
#endif // SURGE_SSESINCDELAYLINE_H
