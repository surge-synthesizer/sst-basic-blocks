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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MOD_MATRIX_MODMATRIXDETAILS_H
#define INCLUDE_SST_BASIC_BLOCKS_MOD_MATRIX_MODMATRIXDETAILS_H

#include <type_traits>
#include <cstdint>

namespace sst::basic_blocks::mod_matrix::details
{

// Thanks again jatin!
#define HAS_MEMBER(M)                                                                              \
    template <typename T> class has_##M                                                            \
    {                                                                                              \
        using No = uint8_t;                                                                        \
        using Yes = uint64_t;                                                                      \
        static_assert(sizeof(No) != sizeof(Yes));                                                  \
        template <typename C> static Yes test(decltype(C::M) *);                                   \
        template <typename C> static No test(...);                                                 \
                                                                                                   \
      public:                                                                                      \
        enum                                                                                       \
        {                                                                                          \
            value = sizeof(test<T>(nullptr)) == sizeof(Yes)                                        \
        };                                                                                         \
    };

HAS_MEMBER(isTargetModMatrixDepth)
HAS_MEMBER(supportsLag)
HAS_MEMBER(providesTargetRanges)
HAS_MEMBER(getTargetModMatrixElement)
HAS_MEMBER(getCurveOperator)
#undef HAS_MEMBER

struct detailTypeNo
{
};
template <typename T, typename Arg> detailTypeNo operator==(const T &, const Arg &);

template <typename T, typename Arg = T> struct has_operator_equal
{
    enum
    {
        value =
            !std::is_same<decltype(std::declval<T>() == std::declval<Arg>()), detailTypeNo>::value
    };
};

template <typename TR> struct CheckModMatrixConstraints
{
    static_assert(std::is_constructible<typename TR::SourceIdentifier>::value,
                  "Source Identifier must be constructable");
    static_assert(std::is_constructible<std::hash<typename TR::SourceIdentifier>>::value,
                  "Source Identifier must implement std::hash");
    static_assert(has_operator_equal<typename TR::SourceIdentifier>::value,
                  "Source Identifier must implement operator ==");

    static_assert(std::is_constructible<typename TR::TargetIdentifier>::value,
                  "Target Identifier must be constructable");
    static_assert(std::is_constructible<std::hash<typename TR::TargetIdentifier>>::value,
                  "Target Identifier must implement std::hash");
    static_assert(has_operator_equal<typename TR::TargetIdentifier>::value,
                  "Target Identifier must implement operator ==");

    static_assert(std::is_constructible<typename TR::CurveIdentifier>::value,
                  "Curve Identifier must be constructable");
    static_assert(std::is_constructible<std::hash<typename TR::CurveIdentifier>>::value,
                  "Curve Identifier must implement std::hash");
    static_assert(has_operator_equal<typename TR::CurveIdentifier>::value,
                  "Curve Identifier must implement operator ==");

    static_assert(std::is_same<decltype(TR::IsFixedMatrix), const bool>::value,
                  "Configuration must define IsFixedMatrix const bool");

    static_assert(std::is_constructible<typename TR::RoutingExtraPayload>::value,
                  "RoutingExtraPayload must be a constructible type");

    // Self Modulation
    static_assert(!has_isTargetModMatrixDepth<TR>::value ||
                      (TR::IsFixedMatrix && has_getTargetModMatrixElement<TR>::value),
                  "Self-targeting requires fixed matrix with getTargetModMatrixElement for now");
};
} // namespace sst::basic_blocks::mod_matrix::details

#endif // SHORTCIRCUITXT_MODMATRIXDETAILS_H
