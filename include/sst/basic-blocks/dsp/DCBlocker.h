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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_DCBLOCKER_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_DCBLOCKER_H

#include <cstdint>

namespace sst::basic_blocks::dsp
{
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

    inline void filter(float *from, float *to) // BLOCK_SIZE
    {
        for (auto i = 0; i < blocksize; ++i)
        {
            auto dx = from[i] - xN1;
            auto fv = dx + fac * yN1;

            xN1 = from[i];
            yN1 = fv;

            to[i] = fv;
        }
    }
};

} // namespace sst::basic_blocks::dsp
#endif // SURGE_MIDSIDE_H
