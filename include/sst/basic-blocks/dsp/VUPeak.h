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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_VUPEAK_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_VUPEAK_H

#include <algorithm>
#include <cmath>

#include "sst/basic-blocks/mechanics/block-ops.h"

namespace sst::basic_blocks::dsp
{
struct VUPeak
{
    float sampleRate{0};
    float falloff{1};

    float vu_peak[2]{0, 0};

    explicit VUPeak() {}
    void setSampleRate(float sr)
    {
        sampleRate = sr;

        // This factor used to be hardcoded to 0.997, which thanks to some reverse engineering
        // by @selenologist was deduced to be the factor of a one pole lowpass with a ~21 Hz cutoff.
        // (Please refer to the VU falloff calc at the end of process() method in
        // SurgeSynthesizer.cpp!) But this made the meters fall off at different rates depending on
        // sample rate we're running at, so we unrolled this into an equation to make the VU meters
        // have identical ballistics at any sample rate! We have also decided to make the meters
        // less sluggish, so we increased the cutoff to 60 Hz
        falloff = (float)std::exp(-2 * M_PI * (60.f / sampleRate));
    }

    void process(float L, float R)
    {
        vu_peak[0] = std::min(2.f, falloff * vu_peak[0]);
        vu_peak[1] = std::min(2.f, falloff * vu_peak[1]);
        vu_peak[0] = std::max(vu_peak[0], std::fabs(L));
        vu_peak[1] = std::max(vu_peak[1], std::fabs(R));
    }

    template <int BLOCK_SIZE> void process(float *L, float *R)
    {
        namespace mech = sst::basic_blocks::mechanics;
        vu_peak[0] = std::min(2.f, falloff * vu_peak[0]);
        vu_peak[1] = std::min(2.f, falloff * vu_peak[1]);
        vu_peak[0] = std::max(vu_peak[0], mech::blockAbsMax<BLOCK_SIZE>(L));
        vu_peak[1] = std::max(vu_peak[1], mech::blockAbsMax<BLOCK_SIZE>(R));
    }
};
} // namespace sst::basic_blocks::dsp

#endif // CONDUIT_VUPEAK_H
