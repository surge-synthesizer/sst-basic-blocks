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

#ifndef SST_BASIC_BLOCKS_DSP_CORRELATEDNOISE_H
#define SST_BASIC_BLOCKS_DSP_CORRELATEDNOISE_H

#include <functional>
#include <cmath>

namespace sst::basic_blocks::dsp
{

inline float correlated_noise_o2mk2_supplied_value(float &lastval, float &lastval2, float correlation,
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
    auto m1 = _mm_rsqrt_ss(_mm_load_ss(&m));
    _mm_store_ss(&m, m1);
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
