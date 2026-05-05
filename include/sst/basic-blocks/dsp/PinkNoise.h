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

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * The pink noise algorithms in this header are "A New Shade of Pink" by
 * Stefan Stenzel (https://github.com/Stenzel/newshadeofpink), released
 * by the original author into the public domain. This file is a
 * single-header C++20 reformulation of the float variants (pink.h and
 * pink-low.h in that repo). Algorithm details:
 * http://www.dspbricks.com/articles/pink-noise/
 * As such this file may be used outside a GPL context
 */

#ifndef INCLUDE_SST_BASIC_BLOCKS_DSP_PINKNOISE_H
#define INCLUDE_SST_BASIC_BLOCKS_DSP_PINKNOISE_H

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace sst::basic_blocks::dsp
{
namespace detail::pink_noise
{
// 12-tap FIR coefficients, factored into two 6-tap halves so each half fits
// in a 64-entry lookup keyed by 6 bits of the LFSR. Tables are computed in
// double then narrowed to float, matching the original C macro precision.
// Coefficients: 1.190566, 0.162580, 0.002208, 0.025475, -0.001522, 0.007322,
//               0.001774, 0.004529, -0.001561, 0.000776, -0.000486, 0.002017
inline constexpr double fcoef(double scale, double c, int m, int shift)
{
    return scale * c * static_cast<double>(2 * ((m >> shift) & 1) - 1);
}
inline constexpr float fa(double scale, float bias, int n)
{
    return static_cast<float>(fcoef(scale, 1.190566, n, 0) + fcoef(scale, 0.162580, n, 1) +
                              fcoef(scale, 0.002208, n, 2) + fcoef(scale, 0.025475, n, 3) +
                              fcoef(scale, -0.001522, n, 4) + fcoef(scale, 0.007322, n, 5) -
                              bias); // first table also subtracts the float-hack bias
}
inline constexpr float fb(double scale, int n)
{
    return static_cast<float>(fcoef(scale, 0.001774, n, 0) + fcoef(scale, 0.004529, n, 1) +
                              fcoef(scale, -0.001561, n, 2) + fcoef(scale, 0.000776, n, 3) +
                              fcoef(scale, -0.000486, n, 4) + fcoef(scale, 0.002017, n, 5));
}

inline constexpr std::array<float, 64> makeFira(double scale, float bias)
{
    std::array<float, 64> r{};
    for (int i = 0; i < 64; ++i)
        r[i] = fa(scale, bias, i);
    return r;
}
inline constexpr std::array<float, 64> makeFirb(double scale)
{
    std::array<float, 64> r{};
    for (int i = 0; i < 64; ++i)
        r[i] = fb(scale, i);
    return r;
}

// Default 18-octave variant: scale 0.0625, bias 3.0
inline constexpr float bias_default = 3.0f;
inline constexpr std::array<float, 64> pfira_default = makeFira(0.0625, bias_default);
inline constexpr std::array<float, 64> pfirb_default = makeFirb(0.0625);

// Low variant: scale halved (0.03125) to make headroom for the extra slow
// octave swapped in via the secondary counter; bias halved to keep the
// float-hack mantissa stable across the wider integration range.
inline constexpr float bias_low = 1.5f;
inline constexpr std::array<float, 64> pfira_low = makeFira(0.03125, bias_low);
inline constexpr std::array<float, 64> pfirb_low = makeFirb(0.03125);

// Bit-reversed mask table indexed by an 8-bit overflowing counter. For each
// block of 16, slot 0 picks one of the slow octaves (the leading n<<7 in the
// original PM16 macro); the rest are fixed mid/upper octaves. Shared by both
// variants.
inline constexpr std::array<uint32_t, 256> pnmask = [] {
    constexpr uint32_t outer[16] = {0u,    0x08u, 0x04u, 0x08u, 0x02u, 0x08u, 0x04u, 0x08u,
                                    0x01u, 0x08u, 0x04u, 0x08u, 0x02u, 0x08u, 0x04u, 0x08u};
    constexpr uint32_t inner[16] = {0u,      0x4000u, 0x2000u, 0x4000u, 0x1000u, 0x4000u,
                                    0x2000u, 0x4000u, 0x0800u, 0x4000u, 0x2000u, 0x4000u,
                                    0x1000u, 0x4000u, 0x2000u, 0x4000u};
    std::array<uint32_t, 256> r{};
    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j)
            r[i * 16 + j] = (j == 0) ? (outer[i] << 7) : inner[j];
    return r;
}();
} // namespace detail::pink_noise

/*
 * 18-octave pink noise generator, after Stenzel "A New Shade of Pink".
 * Multirate construction: 18 binary noise sources clocked at f, f/2, f/4...
 * are summed and shaped by a 12-tap FIR (split into two 6-bit table lookups)
 * to give a flat 1/f spectrum with no multiplies or transcendentals on the
 * audio path. Output is approximately bipolar in [-1, 1].
 *
 * Generates samples in blocks of 16. Caller is responsible for buffering
 * if a different block size is needed.
 */
struct PinkNoise
{
    explicit PinkNoise(uint32_t seed = 0x5EED41F5u) { reset(seed); }

    void reset(uint32_t seed = 0x5EED41F5u)
    {
        plfsr = static_cast<int32_t>(seed ? seed : 0x5EED41F5u);
        paccu = detail::pink_noise::bias_default;
        pncnt = 0;
        pinc = 0x04CCCC;
        pdec = 0x04CCCC;
    }

    void generate16(float *out)
    {
        // Locals encourage register allocation; mirrors the original.
        int32_t inc = pinc;
        int32_t dec = pdec;
        float accu = paccu;
        int32_t lfsr = plfsr;

        auto step = [&](int32_t bitmask) {
            int32_t bit = lfsr >> 31;
            dec &= ~bitmask;
            lfsr <<= 1;
            dec |= inc & bitmask;
            inc ^= bit & bitmask;
            float sample = accu;
            // Float-bits hack: bias keeps the exponent fixed so adding a small
            // int to the bit pattern integrates the mantissa.
            accu = std::bit_cast<float>(std::bit_cast<int32_t>(accu) + (inc - dec));
            lfsr ^= bit & 0x46000001;
            *out++ = sample + detail::pink_noise::pfira_default[lfsr & 0x3F] +
                     detail::pink_noise::pfirb_default[(lfsr >> 6) & 0x3F];
        };

        const int32_t mask = static_cast<int32_t>(detail::pink_noise::pnmask[pncnt++]);

        step(mask);
        step(0x040000);
        step(0x020000);
        step(0x040000);
        step(0x010000);
        step(0x040000);
        step(0x020000);
        step(0x040000);
        step(0x008000);
        step(0x040000);
        step(0x020000);
        step(0x040000);
        step(0x010000);
        step(0x040000);
        step(0x020000);
        step(0x040000);

        pinc = inc;
        pdec = dec;
        paccu = accu;
        plfsr = lfsr;
    }

  private:
    int32_t plfsr{};
    int32_t pinc{};
    int32_t pdec{};
    float paccu{};
    uint8_t pncnt{};
};

/*
 * 20-octave variant of the same algorithm: when the primary mask LUT yields
 * zero (the slowest of the regular octaves, which would not toggle this
 * block), a secondary 256x-slower counter is consulted to inject two further
 * octaves below. Useful if you want flatter response into the sub-Hz region.
 * FIR coefficients are scaled by 1/2 to keep amplitude in range, and the
 * float-hack bias is halved to match.
 */
struct PinkNoiseLow
{
    explicit PinkNoiseLow(uint32_t seed = 0xFEEED1F5u) { reset(seed); }

    void reset(uint32_t seed = 0xFEEED1F5u)
    {
        plfsr = static_cast<int32_t>(seed ? seed : 0xFEEED1F5u);
        paccu = detail::pink_noise::bias_low;
        pncnt = 0;
        plcnt = 0;
        pinc = 0x04CCCC;
        pdec = 0x04CCCC;
    }

    void generate16(float *out)
    {
        int32_t inc = pinc;
        int32_t dec = pdec;
        float accu = paccu;
        int32_t lfsr = plfsr;

        auto step = [&](int32_t bitmask) {
            int32_t bit = lfsr >> 31;
            dec &= ~bitmask;
            lfsr <<= 1;
            dec |= inc & bitmask;
            inc ^= bit & bitmask;
            float sample = accu;
            accu = std::bit_cast<float>(std::bit_cast<int32_t>(accu) + (inc - dec));
            lfsr ^= bit & 0x46000001;
            *out++ = sample + detail::pink_noise::pfira_low[lfsr & 0x3F] +
                     detail::pink_noise::pfirb_low[(lfsr >> 6) & 0x3F];
        };

        int32_t mask = static_cast<int32_t>(detail::pink_noise::pnmask[pncnt++]);
        if (!mask)
            mask = static_cast<int32_t>(detail::pink_noise::pnmask[plcnt++] >> 8);

        step(mask);
        step(0x040000);
        step(0x020000);
        step(0x040000);
        step(0x010000);
        step(0x040000);
        step(0x020000);
        step(0x040000);
        step(0x008000);
        step(0x040000);
        step(0x020000);
        step(0x040000);
        step(0x010000);
        step(0x040000);
        step(0x020000);
        step(0x040000);

        pinc = inc;
        pdec = dec;
        paccu = accu;
        plfsr = lfsr;
    }

  private:
    int32_t plfsr{};
    int32_t pinc{};
    int32_t pdec{};
    float paccu{};
    uint8_t pncnt{};
    uint8_t plcnt{};
};

} // namespace sst::basic_blocks::dsp

#endif // INCLUDE_SST_BASIC_BLOCKS_DSP_PINKNOISE_H
