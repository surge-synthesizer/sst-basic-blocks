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
#include "sst/basic-blocks/tables/ExpTimeProvider.h"
#include "sst/basic-blocks/tables/TemposyncSupport.h"

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

TEST_CASE("ExpTimeProvider TwentyFiveSecondExpTable", "[tables]")
{
    tabl::TwentyFiveSecondExpTable expTime;
    expTime.init();

    tabl::TwoToTheXProvider twox;
    twox.init();

    SECTION("timeInSecondsFromParam matches expected formula")
    {
        for (double p = 0.0; p <= 1.0; p += 0.05)
        {
            double A = expTime.A, B = expTime.B, C = expTime.C, D = expTime.D;
            double expected = (std::exp(A + p * (B - A)) + C) / D;
            REQUIRE(expTime.timeInSecondsFromParam(p) == Approx(expected).margin(1e-12));
        }
    }

    SECTION("LUT rate lookup matches direct computation")
    {
        for (float x = 0.01f; x < 1.0f; x += 0.01f)
        {
            auto timeInSeconds = expTime.timeInSecondsFromParam(x);
            double expectedRate = 1.0 / timeInSeconds;
            float lutRate = expTime.lookupRate(x, twox);
            // Allow small relative error from LUT interpolation
            REQUIRE(lutRate == Approx(expectedRate).epsilon(0.01));
        }
    }

    SECTION("Boundary values are reasonable")
    {
        // At p=0, time should be small (fast envelope)
        auto t0 = expTime.timeInSecondsFromParam(0.0);
        REQUIRE(t0 < 0.01);
        REQUIRE(t0 > 0.0);

        // At p=1, time should be around 25 seconds
        auto t1 = expTime.timeInSecondsFromParam(1.0);
        REQUIRE(t1 > 20.0);
        REQUIRE(t1 < 30.0);
    }
}

TEST_CASE("Temposync ZeroOne", "[tables]")
{
    namespace ts = sst::basic_blocks::tables::temposync;
    using Z = ts::ZeroOne;
    using N = ts::Note;

    SECTION("Shape and exact denominator")
    {
        REQUIRE(Z::nBases == 11);
        REQUIRE(Z::nEntries == 33);
        REQUIRE(Z::denom == 32.f); // 33 - 1, a power of two -> exact index/denom
    }

    SECTION("Entries are strictly ordered by duration")
    {
        for (int i = 1; i < Z::nEntries; ++i)
        {
            INFO("entry " << i << " vs " << (i - 1));
            REQUIRE(Z::entries[i].beats > Z::entries[i - 1].beats);
        }
    }

    SECTION("Note stores a consistent beats value")
    {
        for (int i = 0; i < Z::nEntries; ++i)
        {
            const auto &n = Z::entries[i];
            REQUIRE(n.beats == Approx(N::beatsFor(n.exponent, n.modifier)));
        }
    }

    SECTION("index -> 0..1 -> index is exact for every entry")
    {
        for (int i = 0; i < Z::nEntries; ++i)
            REQUIRE(Z::indexFromZeroOne(Z::zeroOneFromIndex(i)) == i);
    }

    SECTION("Reverse map inverts baseIndexOf/modifierOf")
    {
        for (int i = 0; i < Z::nEntries; ++i)
            REQUIRE(Z::indexFor(Z::baseIndexOf(i), Z::modifierOf(i)) == i);
        for (int b = 0; b < Z::nBases; ++b)
        {
            for (auto mod : {N::Triplet, N::Straight, N::Dotted})
            {
                int idx = Z::indexFor(b, mod);
                REQUIRE(Z::baseIndexOf(idx) == b);
                REQUIRE(Z::modifierOf(idx) == mod);
            }
        }
    }

    SECTION("Quarter convention: exponent 0 == quarter == 1 beat")
    {
        int q = Z::indexFor(0 - Z::minExp, N::Straight); // exponent 0
        REQUIRE(Z::entries[q].exponent == 0);
        REQUIRE(Z::entries[q].beats == Approx(1.0));
        REQUIRE(ts::toStringCompact(Z::entries[q]) == "1/4");
        REQUIRE(ts::secondsFor(Z::entries[q], 60.0) == Approx(1.0));
        REQUIRE(ts::secondsFor(Z::entries[q], 120.0) == Approx(0.5));
    }

    SECTION("Range endpoints and modifier math")
    {
        REQUIRE(ts::toStringCompact(Z::entries[0]) == "1/128 T");
        REQUIRE(Z::entries[0].beats == Approx(1.0 / 48.0)); // 1/32 beat * 2/3
        REQUIRE(ts::toStringCompact(Z::entries[Z::nEntries - 1]) == "12W");
        REQUIRE(Z::entries[Z::nEntries - 1].beats == Approx(48.0)); // 32 beats * 3/2

        int eighth = Z::indexFor(-1 - Z::minExp, N::Straight); // exponent -1 == 1/8 note
        int eighthD = Z::indexFor(-1 - Z::minExp, N::Dotted);
        int eighthT = Z::indexFor(-1 - Z::minExp, N::Triplet);
        REQUIRE(ts::toStringCompact(Z::entries[eighth]) == "1/8");
        REQUIRE(Z::entries[eighthD].beats == Approx(Z::entries[eighth].beats * 1.5));
        REQUIRE(Z::entries[eighthT].beats == Approx(Z::entries[eighth].beats * 2.0 / 3.0));
    }

    SECTION("noteFromValue / valueFromNote round-trip on every entry")
    {
        for (int i = 0; i < Z::nEntries; ++i)
        {
            auto v = Z::zeroOneFromIndex(i);
            REQUIRE(Z::toFloat(Z::fromFloat(v)) == Approx(v));
        }
    }

    SECTION("snap lands on a representable 0..1 value")
    {
        for (float v = 0.f; v <= 1.f; v += 0.013f)
        {
            float sn = Z::snap(v);
            REQUIRE(Z::zeroOneFromIndex(Z::indexFromZeroOne(sn)) == sn);
        }
    }

    SECTION("Full notation table matches the agreed spec")
    {
        static const char *expectedCompact[Z::nEntries] = {
            "1/128 T", "1/128", "1/64 T", "1/128 D", "1/64", "1/32 T", "1/64 D", "1/32", "1/16 T",
            "1/32 D",  "1/16",  "1/8 T",  "1/16 D",  "1/8",  "1/4 T",  "1/8 D",  "1/4",  "1/2 T",
            "1/4 D",   "1/2",   "1W T",   "1/2 D",   "1W",   "2W T",   "1W D",   "2W",   "4W T",
            "3W",      "4W",    "8W T",   "6W",      "8W",   "12W"};
        static const char *expectedVerbose[Z::nEntries] = {
            "1/128 triplet",     "1/128 note",       "1/64 triplet",     "1/128 dotted",
            "1/64 note",         "1/32 triplet",     "1/64 dotted",      "1/32 note",
            "1/16 triplet",      "1/32 dotted",      "1/16 note",        "1/8 triplet",
            "1/16 dotted",       "1/8 note",         "1/4 triplet",      "1/8 dotted",
            "1/4 note",          "1/2 triplet",      "1/4 dotted",       "1/2 note",
            "whole triplet",     "1/2 dotted",       "whole note",       "2 whole triplets",
            "dotted whole note", "2 whole notes",    "4 whole triplets", "3 whole notes",
            "4 whole notes",     "8 whole triplets", "6 whole notes",    "8 whole notes",
            "12 whole notes"};
        for (int i = 0; i < Z::nEntries; ++i)
        {
            INFO("idx " << i);
            REQUIRE(ts::toStringCompact(Z::entries[i]) == expectedCompact[i]);
            REQUIRE(ts::toString(Z::entries[i]) == expectedVerbose[i]);
        }
    }

    SECTION("fromString(toString(n)) round-trips every entry (verbose and compact)")
    {
        for (int i = 0; i < Z::nEntries; ++i)
        {
            auto n = Z::entries[i];
            INFO("verbose entry " << i << " '" << ts::toString(n) << "'");
            auto v = ts::fromString(ts::toString(n));
            REQUIRE(v.has_value());
            REQUIRE(v->exponent == n.exponent);
            REQUIRE(v->modifier == n.modifier);

            INFO("compact entry " << i << " '" << ts::toStringCompact(n) << "'");
            auto c = ts::fromString(ts::toStringCompact(n));
            REQUIRE(c.has_value());
            REQUIRE(c->exponent == n.exponent);
            REQUIRE(c->modifier == n.modifier);
        }
    }

    SECTION("Zero-stage mode puts a true 0 at the bottom index")
    {
        auto z = Z::fromFloat(0.f, true);
        REQUIRE(z.isZero());
        REQUIRE(ts::secondsFor(z, 120.0) == 0.0);
        REQUIRE(ts::toString(z) == "0 s");
        REQUIRE(ts::toStringCompact(z) == "0");
        REQUIRE(Z::toFloat(z, true) == Approx(Z::zeroOneFromIndex(0)));

        // the other 32 slots are unchanged from non-zero mode
        for (int i = 1; i < Z::nEntries; ++i)
        {
            auto n = Z::fromFloat(Z::zeroOneFromIndex(i), true);
            REQUIRE_FALSE(n.isZero());
            REQUIRE(n.exponent == Z::entries[i].exponent);
            REQUIRE(n.modifier == Z::entries[i].modifier);
        }

        // default mode still maps index 0 to the smallest note
        REQUIRE_FALSE(Z::fromFloat(0.f).isZero());
        REQUIRE(Z::fromFloat(0.f).exponent == Z::entries[0].exponent);

        // "0" / "0 s" parse to the zero stage
        REQUIRE(ts::fromString("0").has_value());
        REQUIRE(ts::fromString("0")->isZero());
        REQUIRE(ts::fromString("0 s")->isZero());

        // beatsFromFloat: zero at the bottom, real beats elsewhere
        REQUIRE(Z::beatsFromFloat(0.f, true) == 0.0);
        REQUIRE(Z::beatsFromFloat(Z::zeroOneFromIndex(1), true) == Z::entries[1].beats);
        REQUIRE(Z::beatsFromFloat(0.f, false) == Z::entries[0].beats); // non-zero mode

        // inverseBeatsFromFloat: 1/beats lookup, 0 at the zero stage
        REQUIRE(Z::inverseBeatsFromFloat(0.f, true) == 0.0);
        for (int i = 1; i < Z::nEntries; ++i)
        {
            auto v = Z::zeroOneFromIndex(i);
            REQUIRE(Z::inverseBeatsFromFloat(v, true) == Approx(1.0 / Z::entries[i].beats));
            REQUIRE(Z::inverseBeatsFromFloat(v) == Approx(1.0 / Z::entries[i].beats));
        }
    }

    SECTION("ZeroOne interpolated beats")
    {
        using Z = ts::ZeroOne;

        // Exact at every grid knot: matches the snapped beatsFromFloat there, so
        // an unmodulated (UI-snapped) value glides nowhere.
        for (int i = 0; i < Z::nEntries; ++i)
        {
            auto v = Z::zeroOneFromIndex(i);
            REQUIRE(Z::interpolatedBeatsFromFloat(v, true) == Z::beatsFromFloat(v, true));
            REQUIRE(Z::interpolatedBeatsFromFloat(v, false) == Z::beatsFromFloat(v, false));
        }

        // Boundaries. At 0 with zeroAtBottom it is the zero stage (0 beats); at 0
        // without it is the smallest note; at 1 it is the largest note. All three
        // are clamped knots, so interpolation == the table value exactly.
        REQUIRE(Z::interpolatedBeatsFromFloat(0.f, true) == 0.0);
        REQUIRE(Z::interpolatedBeatsFromFloat(0.f, false) == Z::entries[0].beats);
        REQUIRE(Z::interpolatedBeatsFromFloat(1.f, true) == Z::entries[Z::nEntries - 1].beats);
        REQUIRE(Z::interpolatedBeatsFromFloat(1.f, false) == Z::entries[Z::nEntries - 1].beats);
        // Out of range clamps to the endpoints rather than extrapolating.
        REQUIRE(Z::interpolatedBeatsFromFloat(-0.5f, false) == Z::entries[0].beats);
        REQUIRE(Z::interpolatedBeatsFromFloat(1.5f, false) == Z::entries[Z::nEntries - 1].beats);

        // Midpoint of a cell is the arithmetic mean of its endpoints (linear in
        // beats), and the lowest cell ramps up from 0 under zeroAtBottom.
        {
            auto v0 = Z::zeroOneFromIndex(0);
            auto v1 = Z::zeroOneFromIndex(1);
            auto mid = 0.5f * (v0 + v1);
            REQUIRE(Z::interpolatedBeatsFromFloat(mid, true) ==
                    Approx(0.5 * Z::entries[1].beats)); // (0 + b1)/2
            REQUIRE(Z::interpolatedBeatsFromFloat(mid, false) ==
                    Approx(0.5 * (Z::entries[0].beats + Z::entries[1].beats)));
        }

        // Strictly monotonic increasing across a fine sweep: modulation always
        // moves continuously, never flat or backward.
        double prev = -1.0;
        for (int k = 0; k <= 2000; ++k)
        {
            auto v = k / 2000.f;
            auto b = Z::interpolatedBeatsFromFloat(v, true);
            REQUIRE(b >= prev);
            if (v > 0.f && v < 1.f)
                REQUIRE(b > prev); // strictly increasing in the interior
            prev = b;
        }
    }
}
