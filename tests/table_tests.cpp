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

#include "sst/basic-blocks/tables/DbToLinearProvider.h"
#include "sst/basic-blocks/tables/EqualTuningProvider.h"
#include "sst/basic-blocks/tables/TwoToTheXProvider.h"

namespace tabl = sst::basic_blocks::tables;

TEST_CASE("DB to Linear", "[tables]")
{
    tabl::DbToLinearProvider dbt;
    dbt.init();

    for (float db = -192; db < 10; db += 0.0327)
    {
        INFO("Testing at " << db);
        REQUIRE(dbt.dbToLinear(db) == Approx(pow(10, db / 20.0)).margin(0.01));
    }
}

TEST_CASE("Equal Tuning", "[tables]")
{
    tabl::EqualTuningProvider equal;
    equal.init();

    REQUIRE(equal.note_to_pitch(60) == 32.0);
    REQUIRE(equal.note_to_pitch(12) == 2.0);

    for (int i = 0; i < 128; ++i)
    {
        REQUIRE(equal.note_to_pitch(i) == Approx(pow(2.0, i / 12.f)).margin(1e-5));
    }
}

TEST_CASE("Two to the X Provider", "[tables]")
{
    tabl::TwoToTheXProvider twox;
    twox.init();

    for (float x = -10; x < 10; x += 0.0173)
    {
        REQUIRE(twox.twoToThe(x) == Approx(pow(2.0, x)).margin(1e-5));
    }
}
