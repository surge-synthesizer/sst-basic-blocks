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
#include <atomic>
#include <cstdlib>
#include <new>

#include "sst/basic-blocks/params/ParamMetadata.h"

namespace pmd = sst::basic_blocks::params;

// Heap-allocation instrumentation for the quanta-mode test below. Replacing the global operator
// new lets us count allocations inside a tight window; it is otherwise a plain malloc wrapper, so
// it does not change behaviour for the rest of the test binary.
namespace
{
std::atomic<uint64_t> gAllocCount{0};
bool gAllocCounting{false};
} // namespace
void *operator new(std::size_t n)
{
    if (gAllocCounting)
        gAllocCount.fetch_add(1, std::memory_order_relaxed);
    auto *p = std::malloc(n == 0 ? 1 : n);
    if (!p)
        throw std::bad_alloc();
    return p;
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete(void *p, std::size_t) noexcept { std::free(p); }
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

    SECTION("Linear no alternate with offset")
    {
        auto p = pmd::ParamMetaData().asFloat().withLinearScaleFormatting("%", 1.f, 0.5f);

        REQUIRE(*(p.valueToString(0.2)) == "0.70 %");
        REQUIRE(*(p.valueToString(0.5)) == "1.00 %");
        REQUIRE(*(p.valueToString(0.9)) == "1.40 %");

        std::string em;
        REQUIRE(*(p.valueFromString("0.70 %", em)) == Approx(0.2));
        REQUIRE(*(p.valueFromString("1.00 %", em)) == Approx(0.5));
        REQUIRE(*(p.valueFromString("1.40 %", em)) == Approx(0.9));
    }

    SECTION("Linear no alternate with scale and offset")
    {
        auto p = pmd::ParamMetaData().asFloat().withRange(0, 1).withLinearScaleFormatting(
            "%", 47.5f, 50.f);

        REQUIRE(*(p.valueToString(0.0)) == "50.00 %");
        REQUIRE(*(p.valueToString(0.5)) == "73.75 %");
        REQUIRE(*(p.valueToString(1.0)) == "97.50 %");

        std::string em;
        REQUIRE(*(p.valueFromString("50.00 %", em)) == Approx(0.0));
        REQUIRE(*(p.valueFromString("73.75 %", em)) == Approx(0.5));
        REQUIRE(*(p.valueFromString("97.50 %", em)) == Approx(1.0));
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
        REQUIRE(*(p.valueFromString("4.20", em)) == Approx(4.2f));
    }

    SECTION("Linear alternate below with typein default")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0, 10)
                     .withLinearScaleFormatting("s")
                     .withDisplayRescalingBelow(1.f, 1000.f, "ms")
                     .withAlternateAsDefaultFromStringUnit(true);

        REQUIRE(*(p.valueToString(0.2)) == "200.00 ms");
        REQUIRE(*(p.valueToString(1.2)) == "1.20 s");
        REQUIRE(*(p.valueToString(4.2)) == "4.20 s");

        std::string em;
        REQUIRE(*(p.valueFromString("0.20 s", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("200.00 ms", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("1.20 s", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("4.20 s", em)) == Approx(4.2f));
        REQUIRE(*(p.valueFromString("200", em)) == Approx(0.2f));
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

    SECTION("Linear alternate above Substring")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(100, 10000)
                     .withLinearScaleFormatting("ms")
                     .withDisplayRescalingAbove(1000.f, 0.001f, "s");

        REQUIRE(*(p.valueToString(400)) == "400.00 ms");
        REQUIRE(*(p.valueToString(1100)) == "1.10 s");
        REQUIRE(*(p.valueToString(9840)) == "9.84 s");

        std::string em;
        REQUIRE(*(p.valueFromString("400 ms", em)) == Approx(400.f));
        REQUIRE(*(p.valueFromString("1100 ms", em)) == Approx(1100.f));
        REQUIRE(*(p.valueFromString("1.10 s", em)) == Approx(1100.f));
        REQUIRE(*(p.valueFromString("9840 ms", em)) == Approx(9840.f));
        REQUIRE(*(p.valueFromString("9840", em)) == Approx(9840.f));
        REQUIRE(*(p.valueFromString("9.84 s", em)) == Approx(9840.f));
    }

    SECTION("Two To The alternate above Substring")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(-3, 5)
                     .withATwoToTheBFormatting(1000, 1, "ms")
                     .withDisplayRescalingAbove(1000.f, 0.001f, "s");

        REQUIRE(*(p.valueToString(-1)) == "500.00 ms");
        REQUIRE(*(p.valueToString(-2)) == "250.00 ms");
        REQUIRE(*(p.valueToString(2)) == "4.00 s");

        std::string em;
        REQUIRE(*(p.valueFromString("500 ms", em)) == Approx(-1.f));
        REQUIRE(*(p.valueFromString("2 s", em)) == Approx(1.f));
        REQUIRE(*(p.valueFromString("500", em)) == Approx(-1.f));
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
        REQUIRE(*(p.valueToString(-1.0)) == "500.0 ms");

        std::string em;
        REQUIRE(*(p.valueFromString("1.00 s", em)) == Approx(0.0f));
        REQUIRE(*(p.valueFromString("4.00 0s", em)) == Approx(2.f));
        REQUIRE(*(p.valueFromString("0.50 s", em)) == Approx(-1.f));
        REQUIRE(*(p.valueFromString("500.00 ms", em)) == Approx(-1.f));
    }

    SECTION("With Mlliseconds")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0, 5)
                     .withLinearScaleFormatting("s")
                     .withMilisecondsBelowOneSecond();

        REQUIRE(*(p.valueToString(0.2)) == "200.0 ms"); // alternate decimal places
        REQUIRE(*(p.valueToString(1.2)) == "1.20 s");
        REQUIRE(*(p.valueToString(4.2)) == "4.20 s");

        std::string em;
        REQUIRE(*(p.valueFromString("0.20 s", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("200.00 ms", em)) == Approx(0.2f));
        REQUIRE(*(p.valueFromString("1.20 s", em)) == Approx(1.2f));
        REQUIRE(*(p.valueFromString("4.20 s", em)) == Approx(4.2f));
        REQUIRE(*(p.valueFromString("173", em)) == Approx(0.173f));
    }
}

TEST_CASE("25 Second Exp", "[param]")
{
    SECTION("To String")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        REQUIRE(*(p.valueToString(0.8)) == "3.79 s");
        REQUIRE(*(p.valueToString(0.5)) == "221.6 ms");
    }

    SECTION("From String")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        std::string em;
        // remember these are truncated displays hence the margin
        REQUIRE(*(p.valueFromString("3.79 s", em)) == Approx(0.8f).margin(0.01));
        REQUIRE(*(p.valueFromString("0.22162 s", em)) == Approx(0.5f).margin(0.01));
        REQUIRE(*(p.valueFromString("221.62 ms", em)) == Approx(0.5f).margin(0.01));
        REQUIRE(*(p.valueFromString("221.62", em)) == Approx(0.5f).margin(0.01));
    }

    SECTION("Modulation - large delta stays in seconds (regression: was showing ms)")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        // 100% modulation from 0: delta is ~25 s, must NOT show "25.00 ms"
        auto md = p.modulationNaturalToString(0, 1.0, true, pmd::ParamMetaData::FeatureState());
        REQUIRE(md.has_value());
        REQUIRE(md->value == "25.00 s");
        REQUIRE(md->summary == "+/- 25.00 s");
    }

    SECTION("Modulation - mixed units: large up, small down")
    {
        // base=0.5 (~221.6ms), mod=0.3 → up=~3.57s, down=~-210ms
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        auto md = p.modulationNaturalToString(0.5, 0.3, true, pmd::ParamMetaData::FeatureState());
        REQUIRE(md.has_value());
        REQUIRE(md->value == "3.57 s");
        REQUIRE(md->summary == "+/- 3.57 s");
        REQUIRE(md->valUp == "3.79 s");
        REQUIRE(md->valDown == "11.2 ms");
        REQUIRE(md->baseValue == "221.6 ms");
        REQUIRE(md->singleLineModulationSummary == "11.2 ms < 221.6 ms > 3.79 s");
    }

    SECTION("Modulation - small delta stays in ms")
    {
        // base=0.5 (~221.6ms), mod=0.05 → small delta, both in ms
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        auto md = p.modulationNaturalToString(0.5, 0.05, true, pmd::ParamMetaData::FeatureState());
        REQUIRE(md.has_value());
        // Check units are ms for both
        REQUIRE(md->value.find("ms") != std::string::npos);
        REQUIRE(md->summary.find("ms") != std::string::npos);
    }

    SECTION("Modulation from string round-trip")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        std::string em;
        // modulation that takes base=0 to ~25s should decode to ~1.0 natural units
        auto mv = p.modulationNaturalFromString("25.00 s", 0.f, em);
        REQUIRE(mv.has_value());
        REQUIRE(*mv == Approx(1.0f).margin(0.01f));

        // modulation that takes base=0.5 by ~3.57s should decode back
        auto md = p.modulationNaturalToString(0.5, 0.3, true);
        REQUIRE(md.has_value());
        auto mv2 = p.modulationNaturalFromString(md->value, 0.5f, em);
        REQUIRE(mv2.has_value());
        REQUIRE(*mv2 == Approx(0.3f).margin(0.01f));
    }

    SECTION("Temposync modulation shows percent, not ms/s")
    {
        auto p = pmd::ParamMetaData().as25SecondTemposyncableExpTime();
        auto fs = pmd::ParamMetaData::FeatureState().withTemposync(true);

        // depth is a percent of the 0..1 natural range
        auto md = p.modulationNaturalToString(0.5, 0.3, true, fs);
        REQUIRE(md.has_value());
        REQUIRE(md->value == "30.00 %");
        REQUIRE(md->summary == "+/- 30.00 %");
        REQUIRE(md->changeUp == "30.00");
        REQUIRE(md->changeDown == "-30.00");
        // base/up/down stay in beat-fraction notation
        REQUIRE(md->value.find("ms") == std::string::npos);
        REQUIRE(md->value.find('s') == std::string::npos);

        // unidirectional
        auto mdu = p.modulationNaturalToString(0.2, 0.1, false, fs);
        REQUIRE(mdu.has_value());
        REQUIRE(mdu->value == "10.00 %");
        REQUIRE(mdu->summary == "10.00 %");

        // from-string parses a percent back to natural depth
        std::string em;
        auto mv = p.modulationNaturalFromString("30", 0.5f, em, fs);
        REQUIRE(mv.has_value());
        REQUIRE(*mv == Approx(0.3f).margin(1e-5));

        // round-trip via the displayed value
        auto mv2 = p.modulationNaturalFromString(md->value, 0.5f, em, fs);
        REQUIRE(mv2.has_value());
        REQUIRE(*mv2 == Approx(0.3f).margin(1e-5));
    }
}

TEST_CASE("Temposync type In")
{
    std::vector<std::pair<std::string, std::string>> cases = {
        {"1/32", "1/32 note"},    {"1/16", "1/16 note"},     {"1/8", "1/8 note"},
        {"1/4", "1/4 note"},      {"1/2", "1/2 note"},       {"1", "whole note"},
        {"1W", "whole note"},     {"2", "2 whole notes"},    {"2W", "2 whole notes"},

        {"1/4 d", "1/4 dotted"},  {"1/4 .", "1/4 dotted"},   {"1/4.", "1/4 dotted"},
        {"1/4d", "1/4 dotted"},

        {"1/8 t", "1/8 triplet"}, {"1/16t", "1/16 triplet"},

        {"1/4 t", "1/4 triplet"}, {"1/4t", "1/4 triplet"},   {"1/2t", "1/2 triplet"},
        {"1T", "whole triplet"},
    };
    for (const auto &[in, out] : cases)
    {
        DYNAMIC_SECTION("Convert " << in << " to " << out)
        {
            auto p = pmd::ParamMetaData().asLfoRate();
            auto v = p.valueFromTemposyncNotation(in);
            REQUIRE(v.has_value());
            auto s = p.valueToString(*v, pmd::ParamMetaData::FeatureState().withTemposync(true));
            REQUIRE(s.has_value());
            INFO("Result is " << *s << " " << *v);
            REQUIRE(*s == out);
        }
    }
}

TEST_CASE("Temposync ZERO_ONE flavor dispatch")
{
    namespace ts = sst::basic_blocks::tables::temposync;
    using Z = ts::ZeroOne;

    auto p = pmd::ParamMetaData().asFloat().withRange(0.f, 1.f).temposyncable().withTemposyncFlavor(
        ts::Flavor::ZERO_ONE);
    auto fs = pmd::ParamMetaData::FeatureState().withTemposync(true);

    SECTION("snap + notation dispatch to the ZeroOne/Note path for every grid value")
    {
        for (int i = 0; i < Z::nEntries; ++i)
        {
            float v = Z::zeroOneFromIndex(i);
            REQUIRE(p.snapToTemposync(v) == Approx(Z::snap(v)));
            REQUIRE(p.temposyncNotation(v) == ts::toString(Z::fromFloat(v)));
        }
    }

    SECTION("valueToString renders the ZeroOne note (not the 2^x convention)")
    {
        float q = Z::zeroOneFromIndex(Z::indexFor(0 - Z::minExp, ts::Note::Straight));
        auto s = p.valueToString(q, fs);
        REQUIRE(s.has_value());
        REQUIRE(*s == "1/4 note");
    }

    SECTION("typed compact note decodes through ZeroOne")
    {
        auto v = p.valueFromTemposyncNotation("1/8 T");
        REQUIRE(v.has_value());
        auto n = Z::fromFloat(*v);
        REQUIRE(n.exponent == -1); // 1/8 == exponent -1 on the quarter==0 scale
        REQUIRE(n.modifier == ts::Note::Triplet);
    }

    SECTION("zero-stage flavor renders index 0 as a true zero")
    {
        auto pz = pmd::ParamMetaData()
                      .asFloat()
                      .withRange(0.f, 1.f)
                      .temposyncable()
                      .withTemposyncFlavor(ts::Flavor::ZERO_ONE)
                      .withTemposyncZeroStage();
        auto fz = pmd::ParamMetaData::FeatureState().withTemposync(true);
        REQUIRE(*pz.valueToString(0.f, fz) == "0 s");
        REQUIRE(*pz.valueToString(Z::zeroOneFromIndex(1), fz) == ts::toString(Z::entries[1]));
        // without the zero stage, index 0 is the smallest note
        REQUIRE(*p.valueToString(0.f, fs) == ts::toString(Z::entries[0]));
    }
}

TEST_CASE("Two to the X Formatting")
{
    SECTION("Basic AB")
    {
        auto p = pmd::ParamMetaData().withATwoToTheBFormatting(5, 3, "foo").withRange(0, 3);
        auto v = p.valueToString(3); // so 5 * 2 ^ 3 * 3 = 4 * 2^9 = 2560
        REQUIRE(v.has_value());
        REQUIRE(*v == "2560.00 foo");

        std::string em;
        auto vv = p.valueFromString(*v, em);
        REQUIRE(vv.has_value());
        REQUIRE(*vv == 3);
    }
    SECTION("Basic ABC")
    {
        auto p =
            pmd::ParamMetaData().withATwoToTheBPlusCFormatting(3, 2, -1, "foo").withRange(0, 3);
        auto v = p.valueToString(2); // so 3 * 2 ^ 2*2-1 = 3 * 2^3-1 = 3 * 2^3 == 24
        REQUIRE(v.has_value());
        REQUIRE(*v == "24.00 foo");

        std::string em;
        auto vv = p.valueFromString(*v, em);
        REQUIRE(vv.has_value());
        REQUIRE(*vv == 2);
    }

    SECTION("Basic ABCD")
    {
        auto p = pmd::ParamMetaData()
                     .withATwoToTheBPlusCPlusDFormatting(3, 2, -1, -14, "foo")
                     .withRange(0, 3);
        auto v = p.valueToString(2); // so 3 * 2 ^ 2*2-1  -14 = 3 * 2^3-1 -14 = 3 * 2^3 -14 == 10
        REQUIRE(v.has_value());
        REQUIRE(*v == "10.00 foo");

        std::string em;
        auto vv = p.valueFromString(*v, em);
        REQUIRE(vv.has_value());
        REQUIRE(*vv == 2);
    }

    SECTION("AeB")
    {
        auto p =
            pmd::ParamMetaData().withAExpBPlusCPlusDFormatting(3, 2, 0, 0, "foo").withRange(0, 3);
        auto v = p.valueToString(2); // so 3 e^4
        REQUIRE(v.has_value());
        REQUIRE(*v == "163.79 foo");

        std::string em;
        auto vv = p.valueFromString(*v, em);
        REQUIRE(vv.has_value());
        REQUIRE(*vv == Approx(2).margin(1e-4));
    }

    SECTION("AeBCD")
    {
        auto p = pmd::ParamMetaData()
                     .withAExpBPlusCPlusDFormatting(3, 2, -1, -17, "foo")
                     .withRange(0, 3);
        auto v = p.valueToString(2); // so 3 e^(4-1) - 17
        REQUIRE(v.has_value());
        REQUIRE(*v == "43.26 foo");

        std::string em;
        auto vv = p.valueFromString(*v, em);
        REQUIRE(vv.has_value());
        REQUIRE(*vv == Approx(2).margin(1e-4));
    }

    SECTION("OBXF Log")
    {

        auto logsc = [](float param, const float min, const float max, const float rolloff) {
            return ((expf(param * logf(rolloff + 1.f)) - 1.f) / (rolloff)) * (max - min) + min;
        };
        auto p = pmd::ParamMetaData().withOBXFLogScale(0.2, 17, 23, "foo").withRange(0, 3);
        auto v = p.valueToString(2.1);
        auto vc = logsc(2.1, 0.2, 17, 23);
        REQUIRE(vc == Approx(577.59894));
        REQUIRE(v.has_value());
        REQUIRE(*v == "577.60 foo");
    }
}

TEST_CASE("Modulation Natural To String", "[param]")
{
    SECTION("Linear - symmetric bipolar")
    {
        auto p = pmd::ParamMetaData().asPercent();
        // base=0.5, mod=0.2 bipolar
        auto md = p.modulationNaturalToString(0.5, 0.2, true);
        REQUIRE(md.has_value());
        REQUIRE(md->value == "20.00 %");
        REQUIRE(md->summary == "+/- 20.00 %");
        REQUIRE(md->changeUp == "20.00");
        REQUIRE(md->changeDown == "-20.00");
        REQUIRE(md->valUp == "70.00");
        REQUIRE(md->valDown == "30.00");
    }

    SECTION("Linear - unipolar")
    {
        auto p = pmd::ParamMetaData().asPercent();
        auto md = p.modulationNaturalToString(0.5, 0.2, false);
        REQUIRE(md.has_value());
        REQUIRE(md->summary == "20.00 %");
        REQUIRE(md->changeDown.empty());
        REQUIRE(md->valDown.empty());
    }

    SECTION("Linear from string round-trip")
    {
        auto p = pmd::ParamMetaData().asPercent();
        std::string em;
        auto mv = p.modulationNaturalFromString("20.0 %", 0.5f, em);
        REQUIRE(mv.has_value());
        REQUIRE(*mv == Approx(0.2f).margin(0.001f));

        auto mv2 = p.modulationNaturalFromString("10.0 %", 0.5f, em);
        REQUIRE(mv2.has_value());
        REQUIRE(*mv2 == Approx(0.1f).margin(0.001f));
    }

    SECTION("A_TWO_TO_THE_B (envelope time) modulation display")
    {
        // asEnvelopeTime: 2^val seconds, val in [-8,5]
        // val=0 -> 1s, val=-1 -> 0.5s, val=2 -> 4s
        auto p = pmd::ParamMetaData().asEnvelopeTime();

        // base=0 (1s), mod=1 bipolar -> up to 2s, down to 0.5s
        auto md = p.modulationNaturalToString(0, 1.0, true);
        REQUIRE(md.has_value());
        // du = 2 - 1 = 1 s
        REQUIRE(md->value == "1.00 s");
        REQUIRE(md->summary == "+/- 1.00 s");
        // valUp/valDown are raw display values (not via valueToString in this branch)
        REQUIRE(md->valUp == "2.00");
        REQUIRE(md->valDown == "0.50");
        REQUIRE(md->changeUp == "1.00");
        REQUIRE(md->changeDown == "0.50");
    }

    SECTION("A_TWO_TO_THE_B from string round-trip (envelope time)")
    {
        auto p = pmd::ParamMetaData().asEnvelopeTime();
        std::string em;
        // typing "1.00" should decode to +1 natural unit (takes 1s base to 2s)
        auto mv = p.modulationNaturalFromString("1.00 s", 0.f, em);
        REQUIRE(mv.has_value());
        REQUIRE(*mv == Approx(1.0f).margin(0.01f));

        // round-trip: display -> parse -> same value
        auto md = p.modulationNaturalToString(0, 2.0, true);
        REQUIRE(md.has_value());
        auto mv2 = p.modulationNaturalFromString(md->value, 0.f, em);
        REQUIRE(mv2.has_value());
        REQUIRE(*mv2 == Approx(2.0f).margin(0.01f));
    }

    SECTION("CUBED_AS_DECIBEL modulation display")
    {
        auto p = pmd::ParamMetaData().asCubicDecibelAttenuation();
        // val=1 is 0dB, val=0 is -inf
        // base=1 (0dB), mod=0.1 bipolar
        auto md = p.modulationNaturalToString(1.0, 0.1, true);
        REQUIRE(md.has_value());
        // du = 20*log10(1.1^3) - 20*log10(1.0^3) ≈ 20*3*log10(1.1) ≈ 2.49 dB
        REQUIRE(md->value.find("dB") == std::string::npos); // unit is "dB" but raw delta uses unit
        // Just verify it has valid content and is a decibel delta
        auto vf = std::stof(md->value);
        REQUIRE(vf == Approx(2.49).margin(0.1));
    }

    SECTION("CUBED_AS_DECIBEL from string round-trip")
    {
        auto p = pmd::ParamMetaData().asCubicDecibelAttenuation();
        std::string em;
        // 6dB up from base of 1.0 -> natural offset
        auto mv = p.modulationNaturalFromString("6.0", 1.0f, em);
        REQUIRE(mv.has_value());
        // base=1 (0dB), +6dB → amplitude doubles → val = cbrt(2) - 1 ≈ 0.26
        REQUIRE(*mv == Approx(std::cbrt(2.0) - 1.0).margin(0.01));
    }
}

TEST_CASE("FeatureState withAbsolute", "[param]")
{
    SECTION("withAbsolute does not mutate source (regression)")
    {
        pmd::ParamMetaData::FeatureState fs;
        REQUIRE(!fs.isAbsolute);
        auto fs2 = fs.withAbsolute(true);
        // The original must be unchanged
        REQUIRE(!fs.isAbsolute);
        REQUIRE(fs2.isAbsolute);
    }
}

TEST_CASE("Logarithmic Display Scale", "[param]")
{
    SECTION("To String")
    {
        // 20*log10(val): svA=20, svB=10, svC=0
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0.01f, 1000.f)
                     .withLogarithmicFormating("dB", 20.f, 10.f, 0.f);

        REQUIRE(*(p.valueToString(1.f)) == "0.00 dB");
        REQUIRE(*(p.valueToString(10.f)) == "20.00 dB");
        REQUIRE(*(p.valueToString(100.f)) == "40.00 dB");
        REQUIRE(*(p.valueToString(0.1f)) == "-20.00 dB");
        REQUIRE(*(p.valueToString(0.f)) == "-inf");
    }

    SECTION("From String")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(0.01f, 1000.f)
                     .withLogarithmicFormating("dB", 20.f, 10.f, 0.f);
        std::string em;

        REQUIRE(*(p.valueFromString("0.00 dB", em)) == Approx(1.f).margin(1e-4f));
        REQUIRE(*(p.valueFromString("20.00 dB", em)) == Approx(10.f).margin(1e-4f));
        REQUIRE(*(p.valueFromString("40.00 dB", em)) == Approx(100.f).margin(1e-4f));
        REQUIRE(*(p.valueFromString("-20.00 dB", em)) == Approx(0.1f).margin(1e-4f));
        REQUIRE(*(p.valueFromString("-inf", em)) == Approx(0.01f)); // clamps to minVal
    }
}

TEST_CASE("MIDI Note Display Scale", "[param]")
{
    SECTION("Note names to string")
    {
        auto p = pmd::ParamMetaData().asMIDINote();

        REQUIRE(*(p.valueToString(60)) == "C4");  // middle C
        REQUIRE(*(p.valueToString(69)) == "A4");  // concert A
        REQUIRE(*(p.valueToString(61)) == "C#4"); // C sharp
        REQUIRE(*(p.valueToString(0)) == "C-1");  // lowest MIDI
        REQUIRE(*(p.valueToString(21)) == "A0");  // piano bottom A
        REQUIRE(*(p.valueToString(127)) == "G9"); // highest MIDI
    }

    SECTION("Note names from string")
    {
        auto p = pmd::ParamMetaData().asMIDINote();
        std::string em;

        REQUIRE(*(p.valueFromString("C4", em)) == 60.f);
        REQUIRE(*(p.valueFromString("A4", em)) == 69.f);
        REQUIRE(*(p.valueFromString("C#4", em)) == 61.f);
        REQUIRE(*(p.valueFromString("A0", em)) == 21.f);
    }
}

TEST_CASE("Unordered Map Display", "[param]")
{
    SECTION("On/Off via asOnOffBool")
    {
        auto p = pmd::ParamMetaData().asOnOffBool();

        REQUIRE(*(p.valueToString(0)) == "Off");
        REQUIRE(*(p.valueToString(1)) == "On");
        // Fractional values round to nearest int
        REQUIRE(*(p.valueToString(0.3f)) == "Off");
        REQUIRE(*(p.valueToString(0.7f)) == "On");
    }

    SECTION("Custom map")
    {
        auto p = pmd::ParamMetaData().asInt().withRange(0, 2).withUnorderedMapFormatting(
            {{0, "Slow"}, {1, "Medium"}, {2, "Fast"}});

        REQUIRE(*(p.valueToString(0)) == "Slow");
        REQUIRE(*(p.valueToString(1)) == "Medium");
        REQUIRE(*(p.valueToString(2)) == "Fast");
        REQUIRE(!p.valueToString(3).has_value()); // out of map → nullopt
    }
}

TEST_CASE("Value To Alternate String", "[param]")
{
    SECTION("Exact note names for asAudibleFrequency")
    {
        auto p = pmd::ParamMetaData().asAudibleFrequency();

        // Integer semitone offsets from A4=0 get exact note names (no ~ prefix)
        REQUIRE(*(p.valueToAlternateString(0.f)) == "A4");   // 440 Hz
        REQUIRE(*(p.valueToAlternateString(12.f)) == "A5");  // 880 Hz
        REQUIRE(*(p.valueToAlternateString(-12.f)) == "A3"); // 220 Hz
        REQUIRE(*(p.valueToAlternateString(3.f)) == "C5");   // C5, 3 semitones above A4
    }

    SECTION("Non-integer semitone gets ~ prefix")
    {
        auto p = pmd::ParamMetaData().asAudibleFrequency();
        auto s = p.valueToAlternateString(0.5f);
        REQUIRE(s.has_value());
        REQUIRE(s->substr(0, 1) == "~");
    }

    SECTION("Non-qualifying param returns nullopt")
    {
        auto p = pmd::ParamMetaData().asPercent();
        REQUIRE(!p.valueToAlternateString(0.5f).has_value());
    }
}

TEST_CASE("BOOL Type Normalization", "[param]")
{
    SECTION("natural to normalized01")
    {
        auto p = pmd::ParamMetaData().asBool();
        REQUIRE(p.naturalToNormalized01(0.f) == 0.f);
        REQUIRE(p.naturalToNormalized01(1.f) == 1.f);
        REQUIRE(p.naturalToNormalized01(0.3f) == 0.f); // < 0.5 → false
        REQUIRE(p.naturalToNormalized01(0.7f) == 1.f); // > 0.5 → true
    }

    SECTION("normalized01 to natural")
    {
        auto p = pmd::ParamMetaData().asBool();
        REQUIRE(p.normalized01ToNatural(0.f) == 0.f);
        REQUIRE(p.normalized01ToNatural(1.f) == 1.f);
        REQUIRE(p.normalized01ToNatural(0.3f) == 0.f); // ≤ 0.5 → false
        REQUIRE(p.normalized01ToNatural(0.7f) == 1.f); // > 0.5 → true
    }
}

TEST_CASE("Below One Is Inverse Fraction Feature", "[param]")
{
    // This feature is used in production for the LFO rate multiplier
    SECTION("To string")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(-5, 5)
                     .withATwoToTheBFormatting(1, 1, "x")
                     .withDecimalPlaces(4)
                     .withDefault(0.0)
                     .withFeature(pmd::ParamMetaData::Features::BELOW_ONE_IS_INVERSE_FRACTION);

        REQUIRE(*(p.valueToString(0.f)) == "1.0000 x");    // 2^0 = 1x
        REQUIRE(*(p.valueToString(1.f)) == "2.0000 x");    // 2^1 = 2x
        REQUIRE(*(p.valueToString(-1.f)) == "1/2.0000 x"); // 2^-1 = 0.5 → shown as 1/2
        REQUIRE(*(p.valueToString(-2.f)) == "1/4.0000 x"); // 2^-2 = 0.25 → shown as 1/4
        REQUIRE(*(p.valueToString(2.f)) == "4.0000 x");    // 2^2 = 4x
    }

    SECTION("From string round-trip")
    {
        auto p = pmd::ParamMetaData()
                     .asFloat()
                     .withRange(-5, 5)
                     .withATwoToTheBFormatting(1, 1, "x")
                     .withDecimalPlaces(4)
                     .withDefault(0.0)
                     .withFeature(pmd::ParamMetaData::Features::BELOW_ONE_IS_INVERSE_FRACTION);
        std::string em;

        REQUIRE(*(p.valueFromString("2.0000 x", em)) == Approx(1.f).margin(1e-4f));
        REQUIRE(*(p.valueFromString("1/2.0000 x", em)) == Approx(-1.f).margin(1e-4f));
        REQUIRE(*(p.valueFromString("1/4.0000 x", em)) == Approx(-2.f).margin(1e-4f));
    }
}

TEST_CASE("isNoUnits FeatureState", "[param]")
{
    SECTION("LINEAR suppresses unit")
    {
        auto p = pmd::ParamMetaData().asPercent();
        auto with = p.valueToString(0.5f);
        auto without = p.valueToString(0.5f, pmd::ParamMetaData::FeatureState().withNoUnits(true));
        REQUIRE(*with == "50.00 %");
        REQUIRE(*without == "50.00");
    }

    SECTION("A_TWO_TO_THE_B suppresses unit")
    {
        auto p =
            pmd::ParamMetaData().asFloat().withRange(0, 4).withATwoToTheBFormatting(1, 1, "Hz");
        auto with = p.valueToString(2.f);
        auto without = p.valueToString(2.f, pmd::ParamMetaData::FeatureState().withNoUnits(true));
        REQUIRE(*with == "4.00 Hz");
        REQUIRE(*without == "4.00");
    }

    SECTION("CUBED_AS_DECIBEL suppresses unit")
    {
        auto p = pmd::ParamMetaData().asCubicDecibelAttenuation();
        auto with = p.valueToString(1.f);
        auto without = p.valueToString(1.f, pmd::ParamMetaData::FeatureState().withNoUnits(true));
        REQUIRE(with->find("dB") != std::string::npos);
        REQUIRE(without->find("dB") == std::string::npos);
    }

    SECTION("SCALED_OFFSET_EXP suppresses unit in primary range")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        // val=1 -> 25 s (primary unit range)
        auto with = p.valueToString(1.f);
        auto without = p.valueToString(1.f, pmd::ParamMetaData::FeatureState().withNoUnits(true));
        REQUIRE(with->find("s") != std::string::npos);
        REQUIRE(without->find("s") == std::string::npos);
        REQUIRE(without == "25.00");
    }

    SECTION("SCALED_OFFSET_EXP suppresses unit in alternate (ms) range")
    {
        auto p = pmd::ParamMetaData().as25SecondExpTime();
        // val=0 -> 0 s, which is below the 1 s cutoff -> displays in ms
        auto with = p.valueToString(0.f);
        auto without = p.valueToString(0.f, pmd::ParamMetaData::FeatureState().withNoUnits(true));
        REQUIRE(with->find("ms") != std::string::npos);
        REQUIRE(without->find("ms") == std::string::npos);
        REQUIRE(without == "0.0");
    }
}

TEST_CASE("Custom Min Max and Default")
{
    SECTION("Pan as test case")
    {
        auto p = pmd::ParamMetaData().asPan();
        REQUIRE(p.valueToString(-1).value_or("") == "L");
        REQUIRE(p.valueToString(1).value_or("") == "R");
        REQUIRE(p.valueToString(0).value_or("") == "C");
        REQUIRE(p.valueToString(0.00001).value_or("") == "C");

        std::string em;
        REQUIRE(p.valueFromString("L", em).value_or(1000) == -1);
        REQUIRE(p.valueFromString("R", em).value_or(1000) == 1);
        REQUIRE(p.valueFromString("C", em).value_or(1000) == 0);
    }
}

TEST_CASE("Short Name", "[param]")
{
    SECTION("withName mirrors to empty shortName")
    {
        auto p = pmd::ParamMetaData().withName("Threshold");
        REQUIRE(p.name == "Threshold");
        REQUIRE(p.shortName == "Threshold");
    }

    SECTION("withShortName after withName overrides")
    {
        auto p = pmd::ParamMetaData().withName("Ring Modulation").withShortName("RingMod");
        REQUIRE(p.name == "Ring Modulation");
        REQUIRE(p.shortName == "RingMod");
    }

    SECTION("withName does not clobber existing shortName")
    {
        auto p = pmd::ParamMetaData().withShortName("X").withName("Y");
        REQUIRE(p.name == "Y");
        REQUIRE(p.shortName == "X");
    }

    SECTION("Default shortName is empty")
    {
        auto p = pmd::ParamMetaData();
        REQUIRE(p.shortName.empty());
    }
}

TEST_CASE("Quanta-mode metadata is allocation-free", "[param][quanta]")
{
    using pmd::ParamMetaData;

    // A battery that exercises the allocating cases: long names (past SSO), unit formatting, and a
    // map-formatted param — parameterized by the name and label content. Returns numeric state so
    // nothing is optimized away.
    auto buildChain = [](std::string_view nm, std::string_view lbl) {
        auto a =
            ParamMetaData().asFloat().withRange(-7.f, 7.f).withName(nm).withSemitoneFormatting();
        auto b = ParamMetaData().asInt().withRange(0, 2).withName(nm).withUnorderedMapFormatting(
            {{0, lbl}, {1, lbl}, {2, lbl}});
        return a.maxVal + b.minVal + a.naturalToNormalized01(1.5f) + b.normalized01ToNatural(0.5f);
    };

    // Count heap allocations across one invocation of fn.
    auto count = [](auto &&fn) {
        gAllocCount = 0;
        gAllocCounting = true;
        volatile float sink = fn();
        gAllocCounting = false;
        (void)sink;
        return gAllocCount.load();
    };

    const char *longNm = "A deliberately long parameter name well past any SSO limit";
    const char *longLbl = "A deliberately long enumerated label well past any SSO limit";

    SECTION("quanta ignores string/map content; the full build allocates it")
    {
        // A quanta build skips the string/map stores, so its allocation count depends only on the
        // chain's shape, not the content: long vs empty names/labels allocate identically. (We
        // compare a delta rather than assert zero because MSVC's iterator-debug builds allocate a
        // container proxy per std::vector/std::unordered_map member, unrelated to our stores.)
        uint64_t quantaLong{0}, quantaEmpty{0};
        {
            ParamMetaData::QuantaScope qs;
            quantaLong = count([&] { return buildChain(longNm, longLbl); });
            quantaEmpty = count([&] { return buildChain("", ""); });
        }
        REQUIRE(quantaLong == quantaEmpty);

        // The full build stores the long content, so it allocates strictly more than the quanta
        // one.
        auto fullLong = count([&] { return buildChain(longNm, longLbl); });
        REQUIRE(fullLong > quantaLong);
    }

    SECTION("quanta keeps range/type, drops strings")
    {
        auto full = ParamMetaData()
                        .asInt()
                        .withRange(0, 2)
                        .withName("Character")
                        .withUnorderedMapFormatting({{0, "Warm"}, {1, "Normal"}, {2, "Bright"}});
        ParamMetaData q;
        {
            ParamMetaData::QuantaScope qs;
            q = ParamMetaData()
                    .asInt()
                    .withRange(0, 2)
                    .withName("Character")
                    .withUnorderedMapFormatting({{0, "Warm"}, {1, "Normal"}, {2, "Bright"}});
        }

        REQUIRE(q.quantaOnly);
        REQUIRE_FALSE(full.quantaOnly);
        REQUIRE(q.minVal == full.minVal);
        REQUIRE(q.maxVal == full.maxVal);
        REQUIRE(q.type == full.type);
        REQUIRE(q.name.empty());
        REQUIRE(q.discreteValues.empty());
        REQUIRE_FALSE(full.discreteValues.empty());
        // Normalization matches despite the empty strings/map.
        REQUIRE(q.naturalToNormalized01(1.f) == full.naturalToNormalized01(1.f));
        REQUIRE(q.normalized01ToNatural(0.5f) == full.normalized01ToNatural(0.5f));

        // String conversion declines gracefully on a quanta build (no display metadata), while
        // the full build round-trips normally.
        std::string err;
        REQUIRE(full.valueToString(1.f).has_value());
        REQUIRE_FALSE(q.valueToString(1.f).has_value());

        auto pct = ParamMetaData().asPercent();
        ParamMetaData pctQ;
        {
            ParamMetaData::QuantaScope qs;
            pctQ = ParamMetaData().asPercent();
        }
        REQUIRE(pct.valueToString(0.5f).has_value());
        REQUIRE_FALSE(pctQ.valueToString(0.5f).has_value());
        REQUIRE(pct.valueFromString("50%", err).has_value());
        REQUIRE_FALSE(pctQ.valueFromString("50%", err).has_value());
    }
}