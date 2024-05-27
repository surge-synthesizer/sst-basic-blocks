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

#ifndef INCLUDE_SST_BASIC_BLOCKS_CONCEPTS_UNIT_CONVERSIONS_H
#define INCLUDE_SST_BASIC_BLOCKS_CONCEPTS_UNIT_CONVERSIONS_H

#include <concepts>
#include <type_traits>
#include <cassert>

/*
 * All the names and flavorts we evern given these are here.
 * We obviously need to trim these down once we have everything
 * going. That's tedious but doable because we can just
 * remove the constraint from the concept and the if once we
 * are there.
 */
namespace sst::basic_blocks::concepts
{
template <typename T>
concept has_note_to_pitch_ignoring_tuning = requires(T *s, float f) {
    {
        s->note_to_pitch_ignoring_tuning(f)
    } -> std::floating_point;
};

template <typename T>
concept has_noteToPitchIgnoringTuning = requires(T *s, float f) {
    {
        s->noteToPitchIgnoringTuning(f)
    } -> std::floating_point;
};

template <typename T>
concept has_noteToPitch = requires(T *s, float f) {
    {
        s->noteToPitch(f)
    } -> std::floating_point;
};

template <typename T>
concept has_staticNoteToPitchIgnoringTuning = requires(T *s, float f) {
    {
        T::noteToPitchIgnoringTuning(s, f)
    } -> std::floating_point;
};

template <typename T>
concept has_staticEqualNoteToPitch = requires(T *s, float f) {
    {
        T::equalNoteToPitch(s, f)
    } -> std::floating_point;
};

template <typename T>
concept providesNoteToPitch =
    has_noteToPitchIgnoringTuning<T> || has_note_to_pitch_ignoring_tuning<T> ||
    has_noteToPitch<T> || has_staticNoteToPitchIgnoringTuning<T> || has_staticEqualNoteToPitch<T>;

template <typename T>
    requires(providesNoteToPitch<T>)
inline float convertNoteToPitch(T *t, float n)
{
    if constexpr (has_note_to_pitch_ignoring_tuning<T>)
    {
        return t->note_to_pitch_ignoring_tuning(n);
    }
    else if constexpr (has_noteToPitchIgnoringTuning<T>)
    {
        return t->noteToPitchIgnoringTuning(n);
    }
    else if constexpr (has_noteToPitch<T>)
    {
        return t->noteToPitch(n);
    }
    else if constexpr (has_staticNoteToPitchIgnoringTuning<T>)
    {
        return T::noteToPitchIgnoringTuning(t, n);
    }
    else if constexpr (has_staticEqualNoteToPitch<T>)
    {
        return T::equalNoteToPitch(t, n);
    }

    assert(false);
    return 0;
}

template <typename T>
concept has_db_to_linear = requires(T *s, float f) {
    {
        s->db_to_linear(f)
    } -> std::floating_point;
};

template <typename T>
concept has_dbToLinear = requires(T *s, float f) {
    {
        s->dbToLinear(f)
    } -> std::floating_point;
};

template <typename T>
concept has_staticDbToLinear = requires(T *s, float f) {
    {
        T::dbToLinear(s, f)
    } -> std::floating_point;
};

template <typename T>
concept providesDbToLinear = has_dbToLinear<T> || has_db_to_linear<T> || has_staticDbToLinear<T>;

template <typename T>
    requires(providesDbToLinear<T>)
inline float convertDbToLinear(T *t, float n)
{
    if constexpr (has_db_to_linear<T>)
    {
        return t->db_to_linear(n);
    }
    else if constexpr (has_dbToLinear<T>)
    {
        return t->dbToLinear(n);
    }
    else if constexpr (has_staticDbToLinear<T>)
    {
        return T::dbToLinear(t, n);
    }
    assert(false);
    return 0;
}

} // namespace sst::basic_blocks::concepts
#endif // SHORTCIRCUITXT_UNIT_CONVERSIONS_H
