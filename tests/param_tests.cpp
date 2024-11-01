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
#include <cmath>
#include <iostream>
#include <type_traits>

#include "sst/basic-blocks/params/ParamMetadata.h"

namespace pmd = sst::basic_blocks::params;
TEST_CASE("Percent and BiPolar Percent", "[param]")
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
TEST_CASE("Parameter Constructability", "[param]")
{
    SECTION("Default Constructors")
    {
        static_assert(std::is_copy_constructible_v<pmd::ParamMetaData::FeatureState>);
        static_assert(std::is_move_constructible_v<pmd::ParamMetaData::FeatureState>);
        static_assert(std::is_copy_assignable_v<pmd::ParamMetaData::FeatureState>);
        static_assert(std::is_move_assignable_v<pmd::ParamMetaData::FeatureState>);
    }
}

TEST_CASE("Parameter Polarity", "[param]")
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

TEST_CASE("Extended Float Parameter", "[param]")
{
    SECTION("Simple Extend Value to String")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(-2, 4)
                     .withExtendFactors(10)
                     .withLinearScaleFormatting("whoozits");

        auto nonEV = p.valueToString(0.2);
        REQUIRE(nonEV.has_value());
        REQUIRE(*nonEV == "0.20 whoozits");

        auto ev = p.valueToString(0.2, pmd::ParamMetaData::FeatureState().withExtended(true));
        REQUIRE(ev.has_value());
        REQUIRE(*ev == "2.00 whoozits");
    }

    SECTION("Simple Extend String to Value")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(-2, 4)
                     .withExtendFactors(10)
                     .withLinearScaleFormatting("whoozits");

        std::string emsg;
        auto nonEV = p.valueFromString("0.20 whoozits", emsg);
        REQUIRE(nonEV.has_value());
        REQUIRE(*nonEV == 0.2f);

        auto ev = p.valueFromString("2.00 whoozits", emsg,
                                    pmd::ParamMetaData::FeatureState().withExtended(true));
        REQUIRE(ev.has_value());
        REQUIRE(*ev == 0.2f);
    }

    SECTION("AB Extend Value to String")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0, 4)
                     .withExtendFactors(10, -20)
                     .withLinearScaleFormatting("jimbobs");

        auto nonEV = p.valueToString(0.2);
        REQUIRE(nonEV.has_value());
        REQUIRE(*nonEV == "0.20 jimbobs");

        auto ev = p.valueToString(0.2, pmd::ParamMetaData::FeatureState().withExtended(true));
        REQUIRE(ev.has_value());
        REQUIRE(*ev == "-18.00 jimbobs");
    }

    SECTION("AB Extend String to Value")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0, 4)
                     .withExtendFactors(10, -20)
                     .withLinearScaleFormatting("jimbobs");

        std::string emsg;
        auto nonEV = p.valueFromString("0.20 jimbobs", emsg);
        REQUIRE(nonEV.has_value());
        REQUIRE(*nonEV == 0.2f);

        auto ev = p.valueFromString("-18.00 jimbobs", emsg,
                                    pmd::ParamMetaData::FeatureState().withExtended(true));
        REQUIRE(ev.has_value());
        REQUIRE(*ev == 0.2f);
    }
}

TEST_CASE("Alternate Scales Above and Below", "[param]")
{
    SECTION("Linear no alternate")
    {
        auto p = pmd::ParamMetaData().asFloat().withRange(0, 10).withLinearScaleFormatting("s");

        REQUIRE(*(p.valueToString(0.2)) == "0.20 s");
        REQUIRE(*(p.valueToString(1.2)) == "1.20 s");
        REQUIRE(*(p.valueToString(4.2)) == "4.20 s");

        std::string em;
        REQUIRE(*(p.valueFromString("0.20 s", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("1.20 s", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("4.20 s", em)) == Approx(4.2f));
    }

    SECTION("Linear alternate below")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0, 10)
                     .withLinearScaleFormatting("s")
                     .withDisplayRescalingBelow(1.f, 1000.f, "ms");

        REQUIRE(*(p.valueToString(0.2)) == "200.00 ms");
        REQUIRE(*(p.valueToString(1.2)) == "1.20 s");
        REQUIRE(*(p.valueToString(4.2)) == "4.20 s");

        std::string em;
        REQUIRE(*(p.valueFromString("0.20 s", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("200.00 ms", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("1.20 s", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("4.20 s", em)) == Approx(4.2f));
    }

    SECTION("Linear alternate above")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(100, 10000)
                     .withLinearScaleFormatting("Hz")
                     .withDisplayRescalingAbove(1000.f, 0.001f, "kHz");

        REQUIRE(*(p.valueToString(400)) == "400.00 Hz");
        REQUIRE(*(p.valueToString(1100)) == "1.10 kHz");
        REQUIRE(*(p.valueToString(9840)) == "9.84 kHz");

        std::string em;
        REQUIRE(*(p.valueFromString("400 Hz", em)) == Approx(400.f));
        REQUIRE(*(p.valueFromString("1100 Hz", em)) == Approx(1100.f));
        REQUIRE(*(p.valueFromString("1.10 kHz", em)) == Approx(1100.f));
        REQUIRE(*(p.valueFromString("9840 Hz", em)) == Approx(9840.f));
        REQUIRE(*(p.valueFromString("9.84 kHz", em)) == Approx(9840.f));
    }

    SECTION("Scaled Linear no alternate")
    {
        auto p =
            pmd::ParamMetaData().asFloat().withRange(0, 10).withLinearScaleFormatting("s", 4.f);

        REQUIRE(*(p.valueToString(0.2)) == "0.80 s");
        REQUIRE(*(p.valueToString(1.2)) == "4.80 s");
        REQUIRE(*(p.valueToString(4.2)) == "16.80 s");

        std::string em;
        REQUIRE(*(p.valueFromString("0.80 s", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("4.80 s", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("16.80 s", em)) == Approx(4.2f));
    }

    SECTION("Scaled Linear alternate below")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0, 10)
                     .withLinearScaleFormatting("s", 4.f)
                     .withDisplayRescalingBelow(1.f, 1000.f, "ms");

        REQUIRE(*(p.valueToString(0.2)) == "800.00 ms");
        REQUIRE(*(p.valueToString(1.2)) == "4.80 s");
        REQUIRE(*(p.valueToString(4.2)) == "16.80 s");

        std::string em;
        REQUIRE(*(p.valueFromString("0.80 s", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("800.00 ms", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("4.80 s", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("16.80 s", em)) == Approx(4.2f));
    }

    SECTION("Scaled Linear alternate above")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0, 10)
                     .withLinearScaleFormatting("s", 4.f)
                     .withDisplayRescalingAbove(1.f, 0.1f, "ds");

        REQUIRE(*(p.valueToString(0.2)) == "0.80 s");
        REQUIRE(*(p.valueToString(1.2)) == "0.48 ds");
        REQUIRE(*(p.valueToString(4.2)) == "1.68 ds");

        std::string em;
        REQUIRE(*(p.valueFromString("0.80 s", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("4.80 s", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("0.48 ds", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("16.80 s", em)) == Approx(4.2f));
        REQUIRE(*(p.valueFromString("1.68 ds", em)) == Approx(4.2f));
    }

    SECTION("ABX")
    {
        auto p = pmd::ParamMetaData().asEnvelopeTime().withoutDisplayRescaling();

        REQUIRE(*(p.valueToString(0.0)) == "1.00 s");
        REQUIRE(*(p.valueToString(2.0)) == "4.00 s");
        REQUIRE(*(p.valueToString(-1.0)) == "0.50 s");

        std::string em;
        REQUIRE(*(p.valueFromString("1.00 s", em)) == Approx(0.0f));
        REQUIRE(*(p.valueFromString("4.00 0s", em)) == Approx(2.f));
        REQUIRE(*(p.valueFromString("0.50 s", em)) == Approx(-1.f));
    }

    SECTION("ABX Miliseconds")
    {
        auto p = pmd::ParamMetaData().asEnvelopeTime();

        REQUIRE(*(p.valueToString(0.0)) == "1.00 s");
        REQUIRE(*(p.valueToString(2.0)) == "4.00 s");
        REQUIRE(*(p.valueToString(-1.0)) == "500.00 ms");

        std::string em;
        REQUIRE(*(p.valueFromString("1.00 s", em)) == Approx(0.0f));
        REQUIRE(*(p.valueFromString("4.00 0s", em)) == Approx(2.f));
        REQUIRE(*(p.valueFromString("0.50 s", em)) == Approx(-1.f));
        REQUIRE(*(p.valueFromString("500.00 ms", em)) == Approx(-1.f));
    }
}

TEST_CASE("25 Second Exp", "[param]")
{
    SECTION("To String")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        REQUIRE(*(p.valueToString(0.8)) == "3.79 s");
        REQUIRE(*(p.valueToString(0.5)) == "221.62 ms");
    }

    SECTION("From String")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        std::string em;
        // remember these are truncated displays hence the margin
        REQUIRE(*(p.valueFromString("3.79 s", em)) == Approx(0.8f).margin(0.01));
        REQUIRE(*(p.valueFromString("0.22162 s", em)) == Approx(0.5f).margin(0.01));
        REQUIRE(*(p.valueFromString("221.62 ms", em)) == Approx(0.5f).margin(0.01));
    }

    SECTION("Modulation")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        auto md = p.modulationNaturalToString(0.5, 0.1, true, pmd::ParamMetaData::FeatureState());
        REQUIRE(md.has_value());
    }
}
