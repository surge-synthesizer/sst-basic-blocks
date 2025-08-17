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

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_SMOOTHINGSTRATEGIES_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_SMOOTHINGSTRATEGIES_H

#include "Lag.h"
#include "BlockInterpolators.h"

namespace sst::basic_blocks::dsp
{

struct LagSmoothingStrategy
{
    using smoothValue_t = SurgeLag<double, true>;
    static void setTarget(smoothValue_t &v, float t) { v.newValue(t); }
    static void setValueInstant(smoothValue_t &v, float t)
    {
        v.newValue(t);
        v.instantize();
    }
    static double getValue(smoothValue_t &v) { return v.v; }
    static void process(smoothValue_t &v) { v.process(); }

    static void resetFirstRun(smoothValue_t &v) { v.first_run = true; }
};
template <int blockSize> struct BlockInterpSmoothingStrategy
{
    using smoothValue_t = lipol<double, blockSize, true>;
    static void setTarget(smoothValue_t &v, float t) { v.newValue(t); }
    static void setValueInstant(smoothValue_t &v, float t)
    {
        v.newValue(t);
        v.instantize();
    }
    static double getValue(smoothValue_t &v) { return v.v; }
    static void process(smoothValue_t &v) { v.process(); }
    static void resetFirstRun(smoothValue_t &v) { v.first_run = true; }
};
struct NoSmoothingStrategy
{
    using smoothValue_t = double;
    static void setTarget(smoothValue_t &v, float t) { v = t; }
    static void setValueInstant(smoothValue_t &v, float t) { v = t; }
    static double getValue(smoothValue_t &v) { return v; }
    static void process(smoothValue_t &v) {}

    static void resetFirstRun(smoothValue_t &v) {}
};

} // namespace sst::basic_blocks::dsp
#endif // SMOOTHINGSTRATEGIES_H
