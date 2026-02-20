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

#ifndef INCLUDE_SST_BASIC_BLOCKS_TABLES_EXPTIMEPROVIDER_H
#define INCLUDE_SST_BASIC_BLOCKS_TABLES_EXPTIMEPROVIDER_H

#include <cmath>
#include <algorithm>
#include <concepts>
#include "TwoToTheXProvider.h"

namespace sst::basic_blocks::tables
{

/**
 * Concept for ExpTimeProvider constants: must provide constexpr float A, B, C, D.
 */
template <typename T>
concept ExpTimeConstants = requires {
    { T::A } -> std::convertible_to<float>;
    { T::B } -> std::convertible_to<float>;
    { T::C } -> std::convertible_to<float>;
    { T::D } -> std::convertible_to<float>;
};

/**
 * ExpTimeProvider provides a lookup table for converting a 0..1 parameter
 * into a time value using the exponential mapping:
 *
 *   timeInSeconds = (exp(A + p * (B - A)) + C) / D
 *
 * The LUT stores log2(1/timeInSeconds) so that combined with a TwoToTheXProvider
 * it can efficiently produce rate values for envelope/glide calculations.
 *
 * Template parameter Constants must satisfy ExpTimeConstants (a struct with
 * constexpr float A, B, C, D).
 */
template <ExpTimeConstants Constants> struct ExpTimeProvider
{
    static constexpr double A{Constants::A};
    static constexpr double B{Constants::B};
    static constexpr double C{Constants::C};
    static constexpr double D{Constants::D};

    static constexpr size_t lutSize{1024};
    float lut[lutSize]{};
    bool isInit{false};

    void init()
    {
        if (isInit)
            return;

        for (size_t i = 0; i < lutSize; ++i)
        {
            double x = 1.0 * i / (lutSize - 1);
            auto t = timeInSecondsFromParam(x);
            auto invTime = 1.0 / t;
            lut[i] = (float)std::log2(invTime);
        }

        isInit = true;
    }

    static double timeInSecondsFromParam(double p) { return (std::exp(A + p * (B - A)) + C) / D; }

    /**
     * Given a parameter value x in 0..1, return the rate (1/timeInSeconds)
     * using the LUT and a TwoToTheXProvider for the final pow2.
     */
    float lookupRate(float x, const TwoToTheXProvider &twoToX) const
    {
        auto xp = std::clamp((double)x, 0., 0.9999999999) * (lutSize - 1);
        int xpi = (int)xp;
        auto xpf = xp - xpi;
        auto interp = (1 - xpf) * lut[xpi] + xpf * lut[xpi + 1];
        return twoToX.twoToThe(interp);
    }
};

struct TwentyFiveSecondExpConstants
{
    static constexpr double A{0.6931471824646};
    static constexpr double B{10.1267113685608};
    static constexpr double C{-2.0};
    static constexpr double D{1000.0};
};

using TwentyFiveSecondExpTable = ExpTimeProvider<TwentyFiveSecondExpConstants>;

} // namespace sst::basic_blocks::tables

#endif // INCLUDE_SST_BASIC_BLOCKS_TABLES_EXPTIMEPROVIDER_H
