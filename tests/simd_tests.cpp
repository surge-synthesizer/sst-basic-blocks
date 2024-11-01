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

#include "catch2.hpp"

#include "sst/basic-blocks/simd/setup.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"

#include <iostream>

TEST_CASE("abs_ps", "[simd]")
{
    auto p1 = SIMD_MM(set1_ps)(13.2);
    auto p2 = SIMD_MM(set1_ps)(-142.3);
    auto ap1 = sst::basic_blocks::mechanics::abs_ps(p1);
    auto ap2 = sst::basic_blocks::mechanics::abs_ps(p2);

    float res alignas(16)[4];
    SIMD_MM(store_ps)(res, ap1);
    REQUIRE(res[0] == Approx(13.2).margin(0.0001));

    SIMD_MM(store_ps)(res, ap2);
    REQUIRE(res[0] == Approx(142.3).margin(0.00001));
}

TEST_CASE("Sums", "[simd]")
{
    float res alignas(16)[4];
    res[0] = 0.1;
    res[1] = 0.2;
    res[2] = 0.3;
    res[3] = 0.4;
    auto val = SIMD_MM(load_ps)(res);
    REQUIRE(sst::basic_blocks::mechanics::sum_ps_to_float(val) == Approx(1.0).margin(0.00001));
}