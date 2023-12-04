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
#include "smoke_test_sse.h"
#include <cmath>
#include <iostream>
#include <type_traits>

#include "sst/basic-blocks/params/ParamMetadata.h"

namespace pmd = sst::basic_blocks::params;
TEST_CASE("Percent and BiPolar Percent")
{
    SECTION("Percent")
    {
        auto p = pmd::ParamMetaData().asPercent();
        std::string ems;
        REQUIRE(p.minVal == 0.f);
        REQUIRE(p.maxVal == 1.f);
        REQUIRE(p.naturalToNormalized01(0.37f) == 0.37f);
        REQUIRE(p.supportsStringConversion);
        REQUIRE(p.valueToString(0.731) == "73.10 %");
        REQUIRE(p.valueToString(0.03123456, pmd::ParamMetaData::FeatureState().withHighPrecision(
                                                true)) == "3.123456 %");
        REQUIRE(*(p.valueFromString(*(p.valueToString(0.731)), ems)) == 0.731f);
        REQUIRE(p.modulationNaturalToString(0, 0.2, true)->summary == "+/- 20.00 %");
    }
    SECTION("PercentBipolar")
    {
        auto p = pmd::ParamMetaData().asPercentBipolar();
        std::string ems;

        REQUIRE(p.minVal == -1.f);
        REQUIRE(p.maxVal == 1.f);
        REQUIRE(p.naturalToNormalized01(0.37f) == 0.5 + 0.5 * 0.37f);
        REQUIRE(p.supportsStringConversion);
        REQUIRE(p.valueToString(0.731) == "73.10 %");
        REQUIRE(*(p.valueFromString(*(p.valueToString(0.731)), ems)) == 0.731f);
        REQUIRE(p.modulationNaturalToString(0, 0.2, true)->summary == "+/- 20.00 %");
    }

    SECTION("Audible Frequency")
    {
        auto p = pmd::ParamMetaData().asAudibleFrequency();

        std::string ems;
        REQUIRE(p.supportsStringConversion);
        REQUIRE(*(p.valueToString(0)) == "440.00 Hz");
        REQUIRE(*(p.valueToString(12)) == "880.00 Hz");
        REQUIRE(*(p.valueFromString("440", ems)) == 0);
        REQUIRE(*(p.valueFromString("220", ems)) == -12);
    }
}
TEST_CASE("Parameter Constructability")
{
    SECTION("Default Constructors")
    {
        static_assert(std::is_copy_constructible_v<pmd::ParamMetaData::FeatureState>);
        static_assert(std::is_move_constructible_v<pmd::ParamMetaData::FeatureState>);
        static_assert(std::is_copy_assignable_v<pmd::ParamMetaData::FeatureState>);
        static_assert(std::is_move_assignable_v<pmd::ParamMetaData::FeatureState>);
    }
}
TEST_CASE("Parameter Polarity")
{
    SECTION("Polarity Test")
    {
        auto p = pmd::ParamMetaData().withRange(0, 4);
        REQUIRE(p.getPolarity() == pmd::ParamMetaData::Polarity::UNIPOLAR_POSITIVE);
        REQUIRE(p.isUnipolar());
        REQUIRE(!p.isBipolar());
        p = pmd::ParamMetaData().withRange(-4, 4);
        REQUIRE(p.getPolarity() == pmd::ParamMetaData::Polarity::BIPOLAR);
        REQUIRE(p.isBipolar());
        REQUIRE(!p.isUnipolar());

        p = pmd::ParamMetaData().withRange(-4, 0);
        REQUIRE(p.getPolarity() == pmd::ParamMetaData::Polarity::UNIPOLAR_NEGATIVE);
        REQUIRE(p.isUnipolar());
        REQUIRE(!p.isBipolar());

        p = pmd::ParamMetaData().withRange(-4, 7);
        REQUIRE(p.getPolarity() == pmd::ParamMetaData::Polarity::NO_POLARITY);
        REQUIRE(!p.isBipolar());
        REQUIRE(!p.isUnipolar());

        p = pmd::ParamMetaData().withRange(-4, 7).withPolarity(
            pmd::ParamMetaData::Polarity::BIPOLAR);
        REQUIRE(p.getPolarity() == pmd::ParamMetaData::Polarity::BIPOLAR);
    }
}
