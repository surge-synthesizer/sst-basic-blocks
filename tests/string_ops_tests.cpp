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
#include "sst/basic-blocks/mechanics/string-ops.h"

#include <string>
#include <string_view>
#include <functional>
#include <locale>
#include <optional>

namespace mech = sst::basic_blocks::mechanics;

// Conventions are passed explicitly throughout so these assertions mean the same
// thing on a runner with LC_ALL=C as on a German desktop. The ambient-locale
// entry points are exercised separately, only where the answer cannot vary.
// REQUIRE(*opt == x) dereferences without asking, so an unexpected nullopt is
// undefined behaviour rather than a failed assertion - the case would core dump
// instead of telling you what went wrong. Check first, then compare.
static void requireParses(const std::optional<double> &o, double expected)
{
    REQUIRE(o.has_value());
    REQUIRE(*o == Approx(expected));
}

static const mech::SeparatorConventions dotDecimal{'.', ',', true};   // en_US style
static const mech::SeparatorConventions commaDecimal{',', '.', true}; // de_DE style
static const mech::SeparatorConventions unknown{'.', ',', false};     // classic, no grouping

TEST_CASE("Number parsing accepts either decimal separator")
{
    // The reported bug: atof follows LC_NUMERIC, so one of these silently lost
    // its fraction depending on which locale the process happened to be in.
    for (const auto &conv : {dotDecimal, commaDecimal, unknown})
    {
        requireParses(mech::parseNumber("0.5", conv), 0.5);
        requireParses(mech::parseNumber("0,5", conv), 0.5);
        requireParses(mech::parseNumber("-6,5", conv), -6.5);
        requireParses(mech::parseNumber("-6.5", conv), -6.5);
        requireParses(mech::parseNumber("+1,5", conv), 1.5);
    }
}

TEST_CASE("Two different separators resolve without needing the locale")
{
    // Whichever separator comes last is the decimal point, so these are
    // unambiguous no matter what the platform thinks.
    for (const auto &conv : {dotDecimal, commaDecimal, unknown})
    {
        requireParses(mech::parseNumber("1,234.5", conv), 1234.5);
        requireParses(mech::parseNumber("1.234,5", conv), 1234.5);
    }
}

TEST_CASE("A repeated separator is grouping")
{
    for (const auto &conv : {dotDecimal, commaDecimal, unknown})
    {
        requireParses(mech::parseNumber("1.234.567", conv), 1234567.0);
        requireParses(mech::parseNumber("1,234,567", conv), 1234567.0);
    }
}

TEST_CASE("A lone separator is decimal unless it sits in a group position")
{
    SECTION("locale that groups with a comma")
    {
        // three trailing digits and the locale's own group character
        requireParses(mech::parseNumber("1,234", dotDecimal), 1234.0);
        // one trailing digit cannot be a thousands group
        requireParses(mech::parseNumber("0,5", dotDecimal), 0.5);
        // the other character is not this locale's group character
        requireParses(mech::parseNumber("1.234", dotDecimal), 1.234);
    }

    SECTION("locale that groups with a period")
    {
        requireParses(mech::parseNumber("1.234", commaDecimal), 1234.0);
        requireParses(mech::parseNumber("0.5", commaDecimal), 0.5);
        requireParses(mech::parseNumber("1,234", commaDecimal), 1.234);
    }

    SECTION("locale that tells us nothing falls back to decimal")
    {
        requireParses(mech::parseNumber("1,234", unknown), 1.234);
        requireParses(mech::parseNumber("1.234", unknown), 1.234);
    }
}

TEST_CASE("Number parsing reads a prefix and ignores the rest")
{
    // ParamMetaData leans on this - unit-suffixed type-ins have to keep working.
    for (const auto &conv : {dotDecimal, commaDecimal, unknown})
    {
        requireParses(mech::parseNumber("0.20 whoozits", conv), 0.2);
        requireParses(mech::parseNumber("0,20 s", conv), 0.2);
        requireParses(mech::parseNumber("3 whoozits", conv), 3.0);
        requireParses(mech::parseNumber("50.00 %", conv), 50.0);
    }
}

TEST_CASE("Number parsing handles exponents and leading blanks")
{
    for (const auto &conv : {dotDecimal, commaDecimal, unknown})
    {
        requireParses(mech::parseNumber("1e3", conv), 1000.0);
        requireParses(mech::parseNumber("2,5e-2", conv), 0.025);
        requireParses(mech::parseNumber("  0,5", conv), 0.5);
    }
}

TEST_CASE("Number parsing returns nullopt rather than throwing")
{
    for (const auto &conv : {dotDecimal, commaDecimal, unknown})
    {
        for (const auto &bad : {"banana", "", "   ", ".", ",", "-", "whoozits 3"})
        {
            std::optional<double> r;
            REQUIRE_NOTHROW(r = mech::parseNumber(bad, conv));
            REQUIRE(!r.has_value());
        }
    }
}

TEST_CASE("Separator conventions read from a locale")
{
    // Which locales exist varies by machine and CI image, so each case checks the
    // locale is there before asserting anything - a locale this runner doesn't
    // have should skip rather than fail.
    auto withLocale = [](const char *name, const std::function<void(const std::locale &)> &fn) {
        try
        {
            std::locale loc(name);
            fn(loc);
        }
        catch (const std::exception &)
        {
            WARN("locale " << name << " is not installed here, skipping");
        }
    };

    SECTION("grouping characters other than comma and period")
    {
        // Some locales group with a space rather than a period - macOS reports
        // 0x20 for fr_FR. Which character a given platform reports is not
        // portable, so the behaviour is pinned with explicit conventions here
        // rather than through whatever the runner's fr_FR happens to say.
        mech::SeparatorConventions spaceGrouped{',', ' ', true};
        requireParses(mech::parseNumber("1 234,5", spaceGrouped), 1234.5);
        requireParses(mech::parseNumber("0,5", spaceGrouped), 0.5);

        mech::SeparatorConventions apostropheGrouped{'.', '\'', true};
        requireParses(mech::parseNumber("1'234.5", apostropheGrouped), 1234.5);
    }

    SECTION("the classic locale reports no grouping")
    {
        auto c = mech::separatorConventionsFor(std::locale::classic());
        REQUIRE(c.decimal == '.');
        REQUIRE(!c.grouped);
    }

    SECTION("a dot-decimal locale")
    {
        withLocale("en_US.UTF-8", [](const std::locale &loc) {
            auto c = mech::separatorConventionsFor(loc);
            REQUIRE(c.decimal == '.');
            REQUIRE(c.grouped);

            requireParses(mech::parseNumber("1.5", c), 1.5);
            // one trailing digit cannot be a thousands group, so this is a half
            // whatever this platform uses as its group character
            requireParses(mech::parseNumber("0,5", c), 0.5);
        });
    }

    SECTION("a comma-decimal locale")
    {
        withLocale("fr_FR.UTF-8", [](const std::locale &loc) {
            auto c = mech::separatorConventionsFor(loc);
            REQUIRE(c.decimal == ',');
            REQUIRE(c.grouped);

            requireParses(mech::parseNumber("0,5", c), 0.5);
            // '.' is not this locale's decimal, and one trailing digit is too
            // few to be a group, so this reads as a decimal either way
            requireParses(mech::parseNumber("1.5", c), 1.5);
        });
    }

    SECTION("a period-grouping locale")
    {
        withLocale("de_DE.UTF-8", [](const std::locale &loc) {
            auto c = mech::separatorConventionsFor(loc);
            REQUIRE(c.decimal == ',');

            requireParses(mech::parseNumber("0,5", c), 0.5);
            // two different separators resolve without consulting the locale
            requireParses(mech::parseNumber("1.234,5", c), 1234.5);
        });
    }
}

TEST_CASE("Number parsing overloads")
{
    // string, string_view and char pointer, per the shape asked for in review
    std::string owned{"0,5"};
    std::string_view viewed{owned};

    requireParses(mech::parseNumber(owned, dotDecimal), 0.5);
    requireParses(mech::parseNumber(viewed, dotDecimal), 0.5);
    requireParses(mech::parseNumber("0,5", dotDecimal), 0.5);

    SECTION("a null pointer is not a number")
    {
        const char *nothing{nullptr};
        std::optional<double> r;
        REQUIRE_NOTHROW(r = mech::parseNumber(nothing, dotDecimal));
        REQUIRE(!r.has_value());
    }

    SECTION("the ambient-locale entry points agree on unambiguous input")
    {
        // no separator to disagree about, so these hold under any locale
        requireParses(mech::parseNumber("42"), 42.0);
        requireParses(mech::parseNumber(std::string("42")), 42.0);
        requireParses(mech::parseNumber(std::string_view("42")), 42.0);
        REQUIRE(!mech::parseNumber("banana").has_value());
    }
}
