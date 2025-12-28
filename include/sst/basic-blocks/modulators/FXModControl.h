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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_FXMODCONTROL_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_FXMODCONTROL_H

#if defined(__SSE2__) || defined(_M_AMD64) || defined(_M_X64) ||                                   \
(defined(_M_IX86_FP) && _M_IX86_FP >= 2)
#include <emmintrin.h>
#else
#define SIMDE_ENABLE_NATIVE_ALIASES
#include "simde/x86/sse4.2.h"
#endif

#include <cmath>
#include <utility>

#include "sst/basic-blocks/dsp/BlockInterpolators.h"
#include "sst/basic-blocks/dsp/RNG.h"
#include "sst/basic-blocks/simd/setup.h"
#include "sst/basic-blocks/mechanics/simd-ops.h"

namespace sst::basic_blocks::modulators
{

#define ADD(a, b) SIMD_MM(add_ps)(a, b)
#define SUB(a, b) SIMD_MM(sub_ps)(a, b)
#define DIV(a, b) SIMD_MM(div_ps)(a, b)
#define MUL(a, b) SIMD_MM(mul_ps)(a, b)
#define SETALL(a) SIMD_MM(set1_ps)(a)

template <int blockSize> struct FXModControl
{
  public:
    float samplerate{0}, samplerate_inv{0};
    FXModControl(float sr, float sri) : samplerate(sr), samplerate_inv(sri)
    {
        lfophase = 0.0f;
        SnH_tgt[0] = 0.0f;
        SnH_tgt[1] = 0.0f;

        for (int i = 0; i < LFO_TABLE_SIZE; ++i)
            sin_lfo_table[i] = sin(2.0 * M_PI * i / LFO_TABLE_SIZE);
    }
    FXModControl() : FXModControl(0, 0) {}

    sst::basic_blocks::dsp::RNG rng;

    void setSampleRate(double sr)
    {
        samplerate = sr;
        samplerate_inv = 1.0 / sr;
    }

    enum mod_waves
    {
        mod_sine = 0,
        mod_tri,
        mod_saw,
        mod_ramp,
        mod_square,
        mod_noise,
        mod_snh,
    };

    inline void processStartOfBlock(int mwave, float rate, float depth, float phase_offset,
                                    float width = 0.f)
    {
        assert(samplerate > 1000);
        bool lforeset = false;
        bool rndreset = false;
        // auto lfoout = SIMD_MM(set_ps)(0, 0, lfoValR.v, lfoValL.v);
        float phofs = fmod(fabs(phase_offset), 1.0);
        float thisrate = std::max(0.f, rate);
        float thisphase;
        float thiswidth = std::clamp(width, 0.f, 1.f);

        if (thisrate > 0)
        {
            lfophase += thisrate;

            if (lfophase > 1)
            {
                lfophase = fmod(lfophase, 1.0);
            }

            thisphase = lfophase + phofs;
        }
        else
        {
            thisphase = phofs;

            if (mwave == mod_noise || mwave == mod_snh)
            {
                thisphase *= 16.f;

                if ((int)thisphase != (int)lfophase)
                {
                    rndreset = true;
                    lfophase = (float)((int)thisphase);
                }
            }
        }

        thisphase = fmod(thisphase, 1.0);

        /* We want to catch the first time that thisphase trips over the threshold. There's a couple
         * of ways to do this (like have a state variable), but this should work just as well. */
        if ((thisrate > 0 && thisphase - thisrate <= 0) || (thisrate == 0 && rndreset))
        {
            lforeset = true;
        }

        switch (mwave)
        {
        case mod_sine:
        {
            float thisphaseR = fmod(thisphase + thiswidth * .5f, 1.0);
            // float ps = thisphase * LFO_TABLE_SIZE;
            auto psSSE =
                SIMD_MM(set_ps)(0.f, 0.f, thisphaseR * LFO_TABLE_SIZE, thisphase * LFO_TABLE_SIZE);

            // int psi = (int)ps;
            auto psiSSE = SIMD_MM(cvtps_epi32(psSSE));
            // int psn = (psi + 1) & LFO_TABLE_MASK;
            auto psnSSE = SIMD_MM(and_si128)(SIMD_MM(add_epi32)(psiSSE, SIMD_MM(set1_epi32)(1)),
                                             LFO_TABLE_MASK_SSE);
            // float psf = ps - psi;
            auto psfSSE = SUB(psSSE, SIMD_MM(cvtepi32_ps)(psiSSE));

            auto v = SIMD_MM(set_ps)(0, 0, sin_lfo_table[SIMD_MM(extract_epi32)(psiSSE, 0)],
                                     sin_lfo_table[SIMD_MM(extract_epi32)(psiSSE, 1)]);
            auto vn = SIMD_MM(set_ps)(0, 0, sin_lfo_table[SIMD_MM(extract_epi32)(psnSSE, 0)],
                                      sin_lfo_table[SIMD_MM(extract_epi32)(psnSSE, 1)]);

            // lfoout = sin_lfo_table[psi] * (1.0 - psf) + psf * sin_lfo_table[psn];
            auto sineish = ADD(MUL(v, SUB(oneSSE, psfSSE)), MUL(psfSSE, vn));

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, sineish);
            lfoValL.newValue(res[0]);
            lfoValR.newValue(res[1]);

            break;
        }
        case mod_tri:
        {
            namespace mech = sst::basic_blocks::mechanics;

            float thisphaseR = fmod(thisphase + thiswidth * .5f, 1.0);
            auto ph = SIMD_MM(set_ps)(0, 0, thisphaseR, thisphase);

            // (2.f * fabs(2.f * phase - 1.f) - 1.f);
            auto trianglish = SUB(MUL(twoSSE, mech::abs_ps(SUB(MUL(twoSSE, ph), oneSSE))), oneSSE);

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, trianglish);
            lfoValL.newValue(res[0]);
            lfoValR.newValue(res[1]);

            break;
        }
        case mod_saw: // Gentler than a pure saw, more like a heavily skewed triangle
        {
            float thisphaseR = fmod(thisphase + thiswidth * .5f, 1.0);
            auto ph = SIMD_MM(set_ps)(0, 0, thisphaseR, thisphase);

            // (thisphase / cutAt) * 2.0f - 1.f;
            auto rise = SUB(MUL(DIV(ph, sawCutSSE), twoSSE), oneSSE);
            // (1 - ((thisphase - cutAt) / (1.0 - cutAt))) * 2.0f - 1.f
            auto fall = SUB(
                MUL(twoSSE, SUB(oneSSE, DIV(SUB(ph, sawCutSSE), SUB(oneSSE, sawCutSSE)))), oneSSE);

            // if phase < the cut point rise, else fall
            auto sawish = SIMD_MM(blendv_ps)(rise, fall, SIMD_MM(cmpgt_ps)(ph, sawCutSSE));

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, sawish);
            lfoValL.newValue(res[0]);
            lfoValR.newValue(res[1]);
            break;
        }
        case mod_ramp:
        {
            float thisphaseR = fmod(thisphase + thiswidth * .5f, 1.0);
            auto ph = SIMD_MM(set_ps)(0, 0, thisphaseR, thisphase);

            // (thisphase / cutAt) * 2.0f - 1.f;
            auto rise = SUB(MUL(DIV(ph, sawCutSSE), twoSSE), oneSSE);
            // (1 - ((thisphase - cutAt) / (1.0 - cutAt))) * 2.0f - 1.f
            auto fall = SUB(
                MUL(twoSSE, SUB(oneSSE, DIV(SUB(ph, sawCutSSE), SUB(oneSSE, sawCutSSE)))), oneSSE);

            // if phase < the cut point rise, else fall
            auto sawish = SIMD_MM(blendv_ps)(rise, fall, SIMD_MM(cmpgt_ps)(ph, sawCutSSE));

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, sawish);
            lfoValL.newValue(-res[0]);
            lfoValR.newValue(-res[1]);
            break;
        }
        case mod_square: // Gentler than a pure square, more like a trapezoid
        {
            /* This one is a little trickier. There are four segments, delineated by the beforeHalf
             * etc members. We set up masks that are true for the current segment only, compute
             * the values for all, and use the masks to select the right value. May seem a bit
             * wasteful of those mults/adds/subs which we only need about 2% of blocks, and we
             * gotta blend 4 times... but like, before the update there was a bunch of
             * branching instead so this is probably about as good
             *
             * Also we used to do these divides anew every block but they are now const and
             * expressed differently in the private section below:
             * auto cutOffset = 0.02f;
             * auto m = 2.f / cutOffset;
             * auto c2 = cutOffset / 2.f;
             *
             * The rise and fall stages were also different lengths. The difference was small
             * so I decided to just fix it when I wrote the SSE version.
             */

            float thisphaseR = fmod(thisphase + thiswidth * .5f, 1.0);
            auto ph = SIMD_MM(set_ps)(0, 0, thisphaseR, thisphase);

            auto maskA = SIMD_MM(cmplt_ps)(ph, beforeHalf);
            auto maskB = SIMD_MM(andnot_ps)(maskA, SIMD_MM(cmple_ps)(ph, halfSSE));
            auto maskC =
                SIMD_MM(and_ps)(SIMD_MM(cmpgt_ps)(ph, halfSSE), SIMD_MM(cmplt_ps)(ph, beforeOne));
            auto maskD = SIMD_MM(cmpge_ps)(ph, beforeOne);

            auto squarish = SIMD_MM(set1_ps)(0.f);
            // 1.f
            squarish = SIMD_MM(blendv_ps)(squarish, oneSSE, maskA);
            // -m * phase + (m / 2);
            squarish =
                SIMD_MM(blendv_ps)(squarish, ADD(MUL(negmSSE, ph), DIV(mSSE, twoSSE)), maskB);
            // -1.f
            squarish = SIMD_MM(blendv_ps)(squarish, negoneSSE, maskC);
            // m * thisphase - m + 1;
            squarish = SIMD_MM(blendv_ps)(squarish, ADD(SUB(MUL(mSSE, ph), mSSE), oneSSE), maskD);

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, squarish);
            lfoValL.newValue(res[0]);
            lfoValR.newValue(res[1]);

            break;
        }
        case mod_snh: // Sample & Hold random
        {
            /*
             * Ok this one has a different strategy. Instead of phase offsets for stereo, we
             * use different random values left and right, and make the width average them out.
             * No SIMD here. Don't see a way where it would help much. It's fine.
             */

            if (lforeset)
            {
                SnH_tgt[0] = rng.unifPM1();
                SnH_tgt[1] = rng.unifPM1();
            }

            thiswidth = thiswidth * thiswidth * thiswidth;
            auto wih = (thiswidth * -1.f + 1.f) * .5f;
            auto lrdistance = wih * (SnH_tgt[1] - SnH_tgt[0]);
            auto left = SnH_tgt[0] + lrdistance;
            auto right = SnH_tgt[1] - lrdistance;

            lfoValL.newValue(left);
            lfoValR.newValue(right);
            break;
        }
        case mod_noise: // noise (Sample & Glide smoothed random)
        {
            if (lforeset)
            {
                SnH_tgt[0] = rng.unifPM1();
                SnH_tgt[1] = rng.unifPM1();
            }

            thiswidth = thiswidth * thiswidth * thiswidth;
            auto wih = (thiswidth * -1.f + 1.f) * .5f;
            auto lrdistance = wih * (SnH_tgt[1] - SnH_tgt[0]);
            auto left = SnH_tgt[0] + lrdistance;
            auto right = SnH_tgt[1] - lrdistance;

            // FIXME - exponential creep up. We want to get there in time related to our rate
            auto cvL = lfoValL.v;
            auto cvR = lfoValR.v;

            // thisphase * 0.98 prevents a glitch when LFO rate is disabled
            // and phase offset is 1, which constantly retriggers S&G
            thisrate = (rate == 0) ? thisphase * 0.98 : thisrate;
            float diffL = (left - cvL) * thisrate * 2;
            float diffR = (right - cvR) * thisrate * 2;

            if (thisrate >= 0.98f) // this means we skip all the time
            {
                lfoValL.newValue(left);
                lfoValR.newValue(right);
            }
            else
            {
                lfoValL.newValue(std::clamp(cvL + diffL, -1.f, 1.f));
                lfoValR.newValue(std::clamp(cvR + diffR, -1.f, 1.f));
            }

            break;
        }
        }

        depthLerp.newValue(depth);
    }

    inline float value() const noexcept { return lfoValL.v * depthLerp.v; }
    inline std::array<float, 2> valueStereo() const noexcept
    {
        std::array res = {lfoValL.v * depthLerp.v, lfoValR.v * depthLerp.v};
        return res;
    }
    inline float nextValueInBlock()
    {
        auto res = this->value();
        process();
        return res;
    }
    inline std::array<float, 2> nextStereoValueInBlock()
    {
        auto res = this->valueStereo();
        process();
        return res;
    }
    inline void process()
    {
        lfoValL.process();
        lfoValR.process();
        depthLerp.process();
    }

  private:
    bool first{true};
    dsp::lipol<float, blockSize, true> lfoValL{};
    dsp::lipol<float, blockSize, true> lfoValR{};
    dsp::lipol<float, blockSize, true> depthLerp{};

    const SIMD_M128 oneSSE{SETALL(1.0)};
    const SIMD_M128 negoneSSE{SETALL(-1.0)};
    const SIMD_M128 twoSSE{SETALL(2.0)};
    const SIMD_M128 negtwoSSE{SETALL(-2.0)};

    float lfophase;
    float SnH_tgt[2];

    static constexpr int LFO_TABLE_SIZE = 8192;
    static constexpr int LFO_TABLE_MASK = LFO_TABLE_SIZE - 1;
    const SIMD_M128I LFO_TABLE_MASK_SSE =
        SIMD_MM(set_epi32)(LFO_TABLE_MASK, LFO_TABLE_MASK, LFO_TABLE_MASK, LFO_TABLE_MASK);
    float sin_lfo_table[LFO_TABLE_SIZE]{};

    const SIMD_M128 sawCutSSE = SETALL(0.98f);

    const SIMD_M128 squareCutSSE = SETALL(0.01f);
    const SIMD_M128 halfSSE{SETALL(0.5)};
    const SIMD_M128 beforeHalf = SUB(halfSSE, squareCutSSE);
    const SIMD_M128 beforeOne = SUB(oneSSE, squareCutSSE);

    const SIMD_M128 mSSE = DIV(oneSSE, squareCutSSE);
    const SIMD_M128 negmSSE = MUL(mSSE, negoneSSE);
};

#undef ADD
#undef SUB
#undef DIV
#undef MUL
#undef SETALL
} // namespace sst::basic_blocks::modulators

#endif // SURGE_FXMODCONTROL_H
