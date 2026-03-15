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

#ifndef SST_BASIC_BLOCK_TESTS_TEST_UTILS_H
#define SST_BASIC_BLOCK_TESTS_TEST_UTILS_H

#include <cmath>
#include "sst/basic-blocks/tables/TwoToTheXProvider.h"

namespace test_utils
{
// A sample-rate provider for use with AHDSRShapedSC and similar modulators in tests.
// Block size 8 at 48kHz. Supports both ENVTIME_2TWOX and ENVTIME_EXP phase strategies.
static constexpr int blockSize{8};

struct TestSRProvider
{
    double sampleRate{48000};
    double sampleRateInv{1.0 / 48000};
    float envelope_rate_linear_nowrap(float f) const
    {
        return (float)(blockSize * sampleRateInv * std::pow(2.f, -f));
    }
    const sst::basic_blocks::tables::TwoToTheXProvider &twoToTheXProvider() const
    {
        static sst::basic_blocks::tables::TwoToTheXProvider t;
        if (!t.isInit)
            t.init();
        return t;
    }
};
} // namespace test_utils

#endif // SST_BASIC_BLOCKS_TESTS_TEST_UTILS_H
