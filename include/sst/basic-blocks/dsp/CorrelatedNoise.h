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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_CORRELATEDNOISE_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_CORRELATEDNOISE_H

#include <functional>
#include <cmath>
#include "sst/basic-blocks/simd/setup.h"

namespace sst::basic_blocks::dsp
{

inline float correlated_noise_o2mk2_supplied_value(float &lastval, float &lastval2,
                                                   float correlation,
                                                   const float bipolarUniformRandValue)
{
    float wf = correlation;
    float wfabs = fabs(wf) * 0.8f;
    // wfabs = 1.f - (1.f-wfabs)*(1.f-wfabs);
    wfabs = (2.f * wfabs - wfabs * wfabs);

    if (wf > 0.f)
        wf = wfabs;
    else
        wf = -wfabs;
    float m = 1.f - wfabs;
    // float m = 1.f/sqrt(1.f-wfabs);
    auto m1 = SIMD_MM(rsqrt_ss)(SIMD_MM(load_ss)(&m));
    SIMD_MM(store_ss)(&m, m1);
    // if (wf>0.f) m *= 1 + wf*8;

    float rand11 = bipolarUniformRandValue;
    lastval2 = rand11 * (1 - wfabs) - wf * lastval2;
    lastval = lastval2 * (1 - wfabs) - wf * lastval;
    return lastval * m;
}

inline float correlated_noise_o2mk2_suppliedrng(float &lastval, float &lastval2, float correlation,
                                                std::function<float()> &urng)
{
    return correlated_noise_o2mk2_supplied_value(lastval, lastval2, correlation, urng());
}
} // namespace sst::basic_blocks::dsp
#endif // SURGEXTRACK_CORRELATEDNOISE_H
