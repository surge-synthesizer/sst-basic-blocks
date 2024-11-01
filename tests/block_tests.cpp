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

#include "sst/basic-blocks/mechanics/block-ops.h"

namespace mech = sst::basic_blocks::mechanics;
#include <iostream>

TEST_CASE("Clear and Copy", "[block]")
{
    SECTION("64")
    {
        static constexpr int bs{64};
        float f alignas(16)[bs];
        float g alignas(16)[bs];
        float h alignas(16)[bs];

        for (int i = 0; i < bs; ++i)
            f[i] = std::sin(i * 0.1);

        mech::copy_from_to<bs>(f, g);
        for (int i = 0; i < bs; ++i)
            REQUIRE(f[i] == g[i]);

        mech::scale_by<bs>(f, g);
        for (int i = 0; i < bs; ++i)
            REQUIRE(f[i] * f[i] == g[i]);

        mech::clear_block<bs>(g);
        for (int i = 0; i < bs; ++i)
            REQUIRE(0 == g[i]);

        mech::accumulate_from_to<bs>(f, g);
        mech::accumulate_from_to<bs>(f, g);
        for (int i = 0; i < bs; ++i)
            REQUIRE(2 * f[i] == g[i]);

        auto am = mech::blockAbsMax<bs>(g);
        int eq{0};
        for (int i = 0; i < bs; ++i)
        {
            REQUIRE(std::fabs(g[i]) <= am);
            eq += std::fabs(g[i]) == am;
        }
        REQUIRE(eq > 0);

        for (int i = 0; i < bs; ++i)
            f[i] = (float)std::sin(i * 0.1);
        mech::clear_block<bs>(g);
        mech::scale_accumulate_from_to<bs>(f, 0.5f, g);
        for (int i = 0; i < bs; ++i)
            REQUIRE(0.5f * f[i] == g[i]);
        mech::scale_accumulate_from_to<bs>(f, 0.25f, g);
        for (int i = 0; i < bs; ++i)
            REQUIRE(0.75f * f[i] == g[i]);
    }
}