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

#ifndef INCLUDE_SST_BASIC_BLOCKS_CONCEPTS_CONCEPTS_H
#define INCLUDE_SST_BASIC_BLOCKS_CONCEPTS_CONCEPTS_H

#include <type_traits>

static_assert(__cplusplus >= 202002L, "sst-basic-blocks requires C++20; please update your build");

namespace sst::basic_blocks::concepts
{
template <class T> constexpr bool is_positive_power_of_two(T x) noexcept
{
    return (x > 0) && ((x & (x - 1)) == 0) && std::is_integral_v<T>;
}
} // namespace sst::basic_blocks::concepts

#endif // SURGE_CONCEPTS_H
