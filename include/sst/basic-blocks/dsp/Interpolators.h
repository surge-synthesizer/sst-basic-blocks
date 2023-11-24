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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_INTERPOLATORS_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_INTERPOLATORS_H

namespace sst::basic_blocks::dsp
{
inline float cubic_ipol(float y0, float y1, float y2, float y3, float mu)
{
    float a0, a1, a2, a3, mu2;

    mu2 = mu * mu;
    a0 = y3 - y2 - y0 + y1;
    a1 = y0 - y1 - a0;
    a2 = y2 - y0;
    a3 = y1;

    return (a0 * mu * mu2 + a1 * mu2 + a2 * mu + a3);
}

inline float quad_bspline(float y0, float y1, float y2, float mu)
{
    return 0.5f * (y2 * (mu * mu) + y1 * (-2 * mu * mu + 2 * mu + 1) + y0 * (mu * mu - 2 * mu + 1));
}
} // namespace sst::basic_blocks::dsp

#endif // SURGEXTRACK_INTERPOLATORS_H
