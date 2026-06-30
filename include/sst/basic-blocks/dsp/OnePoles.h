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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_ONEPOLES_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_ONEPOLES_H

#include <cstdint>
#include <cmath>

namespace sst::basic_blocks::dsp
{

/*
 * The one-pole family. These are deliberately tiny, allocation-free building
 * blocks; for tunable musical filtering use sst-filters 
 */

/*
 * DCBlocker is a leaky-differentiator one-pole highpass:
 *   y[n] = x[n] - x[n-1] + fac * y[n-1]
 * The default fac = 0.9993 puts the corner near 5 Hz at 48k. Use it to strip
 * DC; for a tunable highpass use OnePoleHP.
 */
template <uint32_t blocksize> struct DCBlocker
{
    float xN1{0}, yN1{0};
    float fac{0.9993};
    DCBlocker(float f = 0.9993) : fac(f) { reset(); }
    void reset()
    {
        xN1 = 0.f;
        yN1 = 0.f;
    }

    inline float step(float x)
    {
        auto dx = x - xN1;
        auto fv = dx + fac * yN1;

        xN1 = x;
        yN1 = fv;

        return fv;
    }

    inline void filter(float *from, float *to) // BLOCK_SIZE
    {
        for (auto i = 0; i < blocksize; ++i)
            to[i] = step(from[i]);
    }
};

/*
 * TPT (topology-preserving transform) one-pole lowpass. The bilinear prewarp
 * (g = tan(pi*fc/fs)) places the -3 dB point exactly at fc, with unity DC gain.
 * Call setCutoff before use; reset() zeroes the state but leaves the coeff.
 */
struct OnePoleLP
{
    float a{0.f}; // g / (1 + g)
    float s{0.f}; // integrator state

    void setCutoff(float fc, float sampleRate)
    {
        auto g = std::tan(M_PI * fc / sampleRate);
        a = g / (1.f + g);
    }

    void reset() { s = 0.f; }

    inline float step(float x)
    {
        auto v = (x - s) * a;
        auto lp = v + s;
        s = lp + v;
        return lp;
    }
};

/*
 * TPT one-pole highpass: the same integrator, output is x - lowpass. -3 dB at
 * fc, unity gain well above the corner. Distinct from DCBlocker, whose corner
 * is fixed by fac; use this when the corner must track a parameter.
 */
struct OnePoleHP
{
    float a{0.f}; // g / (1 + g)
    float s{0.f}; // integrator state

    void setCutoff(float fc, float sampleRate)
    {
        auto g = std::tan(M_PI * fc / sampleRate);
        a = g / (1.f + g);
    }

    void reset() { s = 0.f; }

    inline float step(float x)
    {
        auto v = (x - s) * a;
        auto lp = v + s;
        s = lp + v;
        return x - lp;
    }
};

} // namespace sst::basic_blocks::dsp
#endif // INCLUDE_SST_BASIC_BLOCKS_DSP_ONEPOLES_H
