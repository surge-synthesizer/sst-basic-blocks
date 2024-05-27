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

#ifndef INCLUDE_SST_BASIC_BLOCKS_CONCEPTS_SAMPLE_RATE_H
#define INCLUDE_SST_BASIC_BLOCKS_CONCEPTS_SAMPLE_RATE_H

#include <concepts>
#include <type_traits>

namespace sst::basic_blocks::concepts
{
template <typename T>
concept has_samplerate_member = requires(T *t) {
    {
        t->samplerate
    } -> std::convertible_to<float>;
};

template <typename T>
concept has_sampleRate_member = requires(T *t) {
    {
        t->sampleRate
    } -> std::convertible_to<float>;
};

template <typename T>
concept has_getSampleRate_method = requires(T *t) {
    {
        t->getSampleRate()
    } -> std::convertible_to<float>;
};

template <typename T>
concept has_staticGetSampleRate_method = requires(T *t) {
    {
        T::getSampleRate(t)
    } -> std::convertible_to<float>;
};

template <typename T>
concept has_dsamplerate_member = requires(T *t) {
    {
        t->dsamplerate
    } -> std::same_as<double>;
};

template <typename T>
concept has_samplerate_inv_member = requires(T *t) {
    {
        t->samplerate_inv
    } -> std::convertible_to<float>;
};

template <typename T>
concept has_sampleRateInv_member = requires(T *t) {
    {
        t->sampleRateInv
    } -> std::convertible_to<float>;
};

template <typename T>
concept has_getSampleRateInv_method = requires(T *t) {
    {
        t->getSampleRateInv()
    } -> std::convertible_to<float>;
};

template <typename T>
concept has_dsamplerate_inv_member = requires(T *t) {
    {
        t->dsamplerate_inv
    } -> std::same_as<double>;
};

template <typename T>
concept has_staticGetSampleRateInv_method = requires(T *s) {
    {
        T::getSampleRateInv(s)
    } -> std::floating_point;
};

template <typename T>
concept has_staticSampleRateInv_method = requires(T *s) {
    {
        T::sampleRateInv(s)
    } -> std::floating_point;
};
template <typename T>
concept supportsSampleRate = has_samplerate_member<T> || has_sampleRate_member<T> ||
                             has_getSampleRate_method<T> || has_staticGetSampleRate_method<T>;

template <typename T>
concept supportsSampleRateInv =
    has_samplerate_inv_member<T> || has_sampleRateInv_member<T> || has_getSampleRateInv_method<T> ||
    has_staticGetSampleRateInv_method<T> || has_staticSampleRateInv_method<T>;

template <typename T>
concept supportsDoubleSampleRate = has_dsamplerate_member<T> && has_dsamplerate_inv_member<T>;

template <typename T>
inline float getSampleRate(T *t)
    requires(supportsSampleRate<T>)
{
    if constexpr (has_samplerate_member<T>)
        return t->samplerate;
    else if constexpr (has_sampleRate_member<T>)
        return t->sampleRate;
    else if constexpr (has_getSampleRate_method<T>)
        return t->getSampleRate();
    else if constexpr (has_staticGetSampleRate_method<T>)
        return T::getSampleRate(t);
}

template <typename T>
inline float getSampleRateInv(T *t)
    requires(supportsSampleRateInv<T>)
{
    if constexpr (has_samplerate_inv_member<T>)
        return t->samplerate_inv;
    else if constexpr (has_sampleRateInv_member<T>)
        return t->sampleRateInv;
    else if constexpr (has_getSampleRateInv_method<T>)
        return t->getSampleRateInv();
    else if constexpr (has_staticGetSampleRateInv_method<T>)
        return T::getSampleRateInv(t);
    else if constexpr (has_staticSampleRateInv_method<T>)
        return T::sampleRateInv(t);
}

} // namespace sst::basic_blocks::concepts

#endif // SHORTCIRCUITXT_SAMPLE_RATE_H
