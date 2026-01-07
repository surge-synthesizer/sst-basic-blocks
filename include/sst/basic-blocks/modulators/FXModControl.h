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

#include <cmath>
#include <utility>
#include <array>

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

/*
 * The SnH and Noise shapes have three behaviors:
 * A single random number shared between all four outputs,
 * in which case the width param controls a phase offset.
 * 4 independent values arranged as two stereo pairs,
 * where width = 0 averages the two values of each pair.
 * And 4 independent values where width = 0 averages all
 * 4 together.
 */
enum RandomBehavior
{
    rnd_single,
    rnd_dual_stereo,
    rnd_quad
};

template <int blockSize, RandomBehavior RB = rnd_dual_stereo> struct FXModControl
{
  public:
    float samplerate{0}, samplerate_inv{0};
    FXModControl(float sr, float sri) : samplerate(sr), samplerate_inv(sri)
    {
        lfophase = 0.0f;
        SnH_tgt[0] = 0.0f;
        SnH_tgt[1] = 0.0f;
        SnH_tgt[2] = 0.0f;
        SnH_tgt[3] = 0.0f;

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
                                    float width = 1.0)
    {
        assert(samplerate > 1000);
        bool lforeset[4] = {false, false, false, false};
        bool rndreset[4] = {false, false, false, false};
        float thisrate = std::max(0.f, rate);
        std::array<float, 4> thisphase;
        float thiswidth = std::clamp(width, 0.f, 1.f);

        if (thisrate > 0)
        {
            lfophase = fmod(lfophase + thisrate, 1.0);
            thisphase[0] = lfophase + phase_offset;
        }
        else
        {
            thisphase[0] = phase_offset;

            if (mwave == mod_noise || mwave == mod_snh)
            {
                if constexpr (RB == rnd_single)
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        thisphase[i] *= 16.f;

                        if ((int)thisphase[i] != (int)lfophase)
                        {
                            rndreset[i] = true;
                            lfophase = (float)((int)thisphase[i]);
                        }
                    }
                }
                else
                {
                    thisphase[0] *= 16.f;

                    if ((int)thisphase[0] != (int)lfophase)
                    {
                        rndreset[0] = true;
                        lfophase = (float)((int)thisphase[0]);
                    }
                }
            }
        }

        for (int i = 0; i < 4; ++i)
        {
            thisphase[i] = fmod(thisphase[0] + i * thiswidth / 4, 1.f);
            lastKnownPhases[i] = thisphase[i];
        }

        if constexpr (RB == rnd_single)
        {
            for (int i = 0; i < 4; ++i)
            {
                if ((thisrate > 0 && thisphase[i] - thisrate <= 0) ||
                    (thisrate == 0 && rndreset[i]))
                {
                    lforeset[i] = true;
                }
            }
        }
        else
        {
            if ((thisrate > 0 && thisphase[0] - thisrate <= 0) || (thisrate == 0 && rndreset[0]))
            {
                lforeset[0] = true;
            }
        }

        auto phaseSSE = SIMD_MM(set_ps)(thisphase[3], thisphase[2], thisphase[1], thisphase[0]);

        switch (mwave)
        {
        case mod_sine:
        {
            // float ps = thisphase * LFO_TABLE_SIZE;
            auto psSSE = MUL(phaseSSE, LFO_TABLE_SIZE_SSE);

            // int psi = (int)ps;
            auto psiSSE = SIMD_MM(cvttps_epi32(psSSE));
            // int psn = (psi + 1) & LFO_TABLE_MASK;
            auto psnSSE = SIMD_MM(and_si128)(SIMD_MM(add_epi32)(psiSSE, SIMD_MM(set1_epi32)(1)),
                                             LFO_TABLE_MASK_SSE);
            // float psf = ps - psi;
            auto psfSSE = SUB(psSSE, SIMD_MM(cvtepi32_ps)(psiSSE));

            SIMD_M128 v = SIMD_MM(set_ps)(sin_lfo_table[SIMD_MM(extract_epi32)(psiSSE, 3)],
                                          sin_lfo_table[SIMD_MM(extract_epi32)(psiSSE, 2)],
                                          sin_lfo_table[SIMD_MM(extract_epi32)(psiSSE, 1)],
                                          sin_lfo_table[SIMD_MM(extract_epi32)(psiSSE, 0)]);
            SIMD_M128 vn = SIMD_MM(set_ps)(sin_lfo_table[SIMD_MM(extract_epi32)(psnSSE, 3)],
                                           sin_lfo_table[SIMD_MM(extract_epi32)(psnSSE, 2)],
                                           sin_lfo_table[SIMD_MM(extract_epi32)(psnSSE, 1)],
                                           sin_lfo_table[SIMD_MM(extract_epi32)(psnSSE, 0)]);

            // lfoout = sin_lfo_table[psi] * (1.0 - psf) + psf * sin_lfo_table[psn];
            auto sineish = ADD(MUL(v, SUB(oneSSE, psfSSE)), MUL(psfSSE, vn));

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, sineish);
            for (int i = 0; i < 4; ++i)
            {
                lfoVals[i].newValue(res[i]);
            }

            break;
        }
        case mod_tri:
        {
            namespace mech = sst::basic_blocks::mechanics;

            // (2.f * fabs(2.f * phase - 1.f) - 1.f);
            auto trianglish =
                SUB(MUL(twoSSE, mech::abs_ps(SUB(MUL(twoSSE, phaseSSE), oneSSE))), oneSSE);

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, trianglish);
            for (int i = 0; i < 4; ++i)
            {
                lfoVals[i].newValue(res[i]);
            }

            break;
        }
        case mod_saw: // Gentler than a pure saw, more like a heavily skewed triangle
        {
            // (thisphase / cutAt) * 2.0f - 1.f;
            auto rise = SUB(MUL(DIV(phaseSSE, sawCutSSE), twoSSE), oneSSE);
            // (1 - ((thisphase - cutAt) / (1.0 - cutAt))) * 2.0f - 1.f
            auto fall =
                SUB(MUL(twoSSE, SUB(oneSSE, DIV(SUB(phaseSSE, sawCutSSE), SUB(oneSSE, sawCutSSE)))),
                    oneSSE);

            // if phase < the cut point rise, else fall
            auto sawish = SIMD_MM(blendv_ps)(rise, fall, SIMD_MM(cmpgt_ps)(phaseSSE, sawCutSSE));

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, sawish);
            for (int i = 0; i < 4; ++i)
            {
                lfoVals[i].newValue(res[i]);
            }
            break;
        }
        case mod_ramp:
        {
            // (thisphase / cutAt) * 2.0f - 1.f;
            auto rise = SUB(MUL(DIV(phaseSSE, sawCutSSE), twoSSE), oneSSE);
            // (1 - ((thisphase - cutAt) / (1.0 - cutAt))) * 2.0f - 1.f
            auto fall =
                SUB(MUL(twoSSE, SUB(oneSSE, DIV(SUB(phaseSSE, sawCutSSE), SUB(oneSSE, sawCutSSE)))),
                    oneSSE);

            // if phase < the cut point rise, else fall
            auto sawish = SIMD_MM(blendv_ps)(rise, fall, SIMD_MM(cmpgt_ps)(phaseSSE, sawCutSSE));

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, sawish);
            for (int i = 0; i < 4; ++i)
            {
                lfoVals[i].newValue(-res[i]);
            }
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
            auto maskA = SIMD_MM(cmplt_ps)(phaseSSE, beforeHalf);
            auto maskB = SIMD_MM(andnot_ps)(maskA, SIMD_MM(cmple_ps)(phaseSSE, halfSSE));
            auto maskC = SIMD_MM(and_ps)(SIMD_MM(cmpgt_ps)(phaseSSE, halfSSE),
                                         SIMD_MM(cmplt_ps)(phaseSSE, beforeOne));
            auto maskD = SIMD_MM(cmpge_ps)(phaseSSE, beforeOne);

            auto squarish = SIMD_MM(set1_ps)(0.f);
            // 1.f
            squarish = SIMD_MM(blendv_ps)(squarish, oneSSE, maskA);
            // -m * phase + (m / 2);
            squarish =
                SIMD_MM(blendv_ps)(squarish, ADD(MUL(negmSSE, phaseSSE), DIV(mSSE, twoSSE)), maskB);
            // -1.f
            squarish = SIMD_MM(blendv_ps)(squarish, negoneSSE, maskC);
            // m * phase - m + 1;
            squarish =
                SIMD_MM(blendv_ps)(squarish, ADD(SUB(MUL(mSSE, phaseSSE), mSSE), oneSSE), maskD);

            float res alignas(16)[4];
            SIMD_MM(store_ps)(res, squarish);
            for (int i = 0; i < 4; ++i)
            {
                lfoVals[i].newValue(res[i]);
            }

            break;
        }
        case mod_snh: // Sample & Hold random
        {
            /*
             * Ok this one has a different strategy. Instead of phase offsets for stereo, we
             * use different random values left and right, and make the width average them out.
             * No SIMD here. Don't see a way where it would help much. It's fine.
             */

            if constexpr (RB == rnd_quad)
            {
                if (lforeset[0])
                {
                    SnH_sum = 0.f;
                    for (int i = 0; i < 4; ++i)
                    {
                        SnH_tgt[i] = rng.unifPM1();
                        SnH_sum += SnH_tgt[i];
                    }
                }

                thiswidth = thiswidth * thiswidth * thiswidth;
                auto wih = (thiswidth * -1.f + 1.f) * .5f;

                auto SnH_avg = SnH_sum * .25f;

                for (int i = 0; i < 4; ++i)
                {
                    auto diff = wih * (SnH_avg - SnH_tgt[i]);
                    auto nv = SnH_tgt[i] + diff;

                    lfoVals[i].newValue(nv);
                }
            }

            if constexpr (RB == rnd_dual_stereo)
            {
                if (lforeset[0])
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        SnH_tgt[i] = rng.unifPM1();
                    }
                }

                thiswidth = thiswidth * thiswidth * thiswidth;
                auto wih = (thiswidth * -1.f + 1.f) * .5f;

                for (int i = 0; i <= 2; i += 2)
                {
                    auto diff = wih * (SnH_tgt[i + 1] - SnH_tgt[i]);
                    auto nvL = SnH_tgt[i] + diff;
                    auto nvR = SnH_tgt[i + 1] - diff;
                    lfoVals[i].newValue(nvL);
                    lfoVals[i + 1].newValue(nvR);
                }
            }

            if constexpr (RB == rnd_single)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (lforeset[i])
                    {
                        SnH_tgt[i] = rng.unifPM1();
                    }

                    lfoVals[i].newValue(SnH_tgt[i]);
                }
            }

            break;
        }
        case mod_noise: // noise (Sample & Glide smoothed random)
        {
            // thisphase * 0.98 prevents a glitch when LFO rate is disabled
            // and phase offset is 1, which constantly retriggers S&G
            thisrate = (rate == 0) ? thisphase[0] * 0.98f : thisrate;

            if constexpr (RB == rnd_quad)
            {
                if (lforeset[0])
                {
                    SnH_sum = 0.f;
                    for (int i = 0; i < 4; ++i)
                    {
                        SnH_tgt[i] = rng.unifPM1();
                        SnH_sum += SnH_tgt[i];
                    }
                }

                thiswidth = thiswidth * thiswidth * thiswidth;
                auto wih = (thiswidth * -1.f + 1.f) * .5f;

                auto SnH_avg = SnH_sum * .25f;

                for (int i = 0; i < 4; ++i)
                {
                    auto diff = wih * (SnH_avg - SnH_tgt[i]);
                    auto nv = SnH_tgt[i] + diff;

                    auto cv = lfoVals[i].v;
                    float inc = (nv - cv) * thisrate * 2;

                    if (thisrate >= 0.98f) // this means we skip all the time
                    {
                        lfoVals[i].newValue(nv);
                    }
                    else
                    {
                        lfoVals[i].newValue(std::clamp(cv + inc, -1.f, 1.f));
                    }
                }
            }

            if constexpr (RB == rnd_dual_stereo)
            {
                if (lforeset[0])
                {
                    for (int i = 0; i < 4; ++i)
                    {
                        SnH_tgt[i] = rng.unifPM1();
                    }
                }

                thiswidth = thiswidth * thiswidth * thiswidth;
                auto wih = (thiswidth * -1.f + 1.f) * .5f;

                for (int i = 0; i <= 2; i += 2)
                {
                    auto diff = wih * (SnH_tgt[i + 1] - SnH_tgt[i]);
                    auto nvL = SnH_tgt[i] + diff;
                    auto nvR = SnH_tgt[i + 1] - diff;

                    auto cvL = lfoVals[i].v;
                    auto cvR = lfoVals[i + 1].v;

                    float incL = (nvL - cvL) * thisrate * 2;
                    float incR = (nvR - cvR) * thisrate * 2;

                    if (thisrate >= 0.98f) // this means we skip all the time
                    {
                        lfoVals[i].newValue(nvL);
                        lfoVals[i + 1].newValue(nvR);
                    }
                    else
                    {
                        lfoVals[i].newValue(std::clamp(cvL + incL, -1.f, 1.f));
                        lfoVals[i + 1].newValue(std::clamp(cvR + incR, -1.f, 1.f));
                    }
                }
            }

            if constexpr (RB == rnd_single)
            {
                for (int i = 0; i < 4; ++i)
                {
                    if (lforeset[i])
                    {
                        SnH_tgt[i] = rng.unifPM1();
                    }

                    auto nv = SnH_tgt[i];
                    auto cv = lfoVals[i].v;
                    float inc = (nv - cv) * thisrate * 2;

                    if (thisrate >= 0.98f) // this means we skip all the time
                    {
                        lfoVals[i].newValue(nv);
                    }
                    else
                    {
                        lfoVals[i].newValue(std::clamp(cv + inc, -1.f, 1.f));
                    }
                }
            }

            break;
        }
        }

        depthLerp.newValue(depth);
    }

    inline float value() const noexcept { return lfoVals[0].v * depthLerp.v; }
    inline float nextValueInBlock()
    {
        auto res = this->value();
        process();
        return res;
    }

    inline std::array<float, 2> valueStereo() const noexcept
    {
        std::array res = {
            lfoVals[0].v * depthLerp.v,
            lfoVals[1].v * depthLerp.v,
        };
        return res;
    }
    inline std::array<float, 2> nextStereoValueInBlock()
    {
        auto res = this->valueStereo();
        process();
        return res;
    }

    inline std::array<float, 4> valueQuad() const noexcept
    {
        std::array res = {lfoVals[0].v * depthLerp.v, lfoVals[1].v * depthLerp.v,
                          lfoVals[2].v * depthLerp.v, lfoVals[3].v * depthLerp.v};
        return res;
    }
    inline std::array<float, 4> nextQuadValueInBlock()
    {
        auto res = this->valueQuad();
        process();
        return res;
    }

    inline SIMD_M128 valueQuadSSE() const noexcept
    {
        auto res = SIMD_MM(set_ps)(lfoVals[3].v, lfoVals[2].v, lfoVals[1].v, lfoVals[0].v);
        res = MUL(res, SIMD_MM(set1_ps)(depthLerp.v));
        return res;
    }
    inline SIMD_M128 nextQuadValueInBlockSSE()
    {
        auto res = valueQuadSSE();
        process();
        return res;
    }

    inline void process()
    {
        for (int i = 0; i < 4; ++i)
        {
            lfoVals[i].process();
        }
        depthLerp.process();
    }

    inline std::array<float, 4> getLastPhase() { return lastKnownPhases; }

  private:
    bool first{true};
    std::array<dsp::lipol<float, blockSize, true>, 4> lfoVals{};
    dsp::lipol<float, blockSize, true> depthLerp{};

    const SIMD_M128 oneSSE{SETALL(1.0)};
    const SIMD_M128 negoneSSE{SETALL(-1.0)};
    const SIMD_M128 twoSSE{SETALL(2.0)};
    const SIMD_M128 negtwoSSE{SETALL(-2.0)};

    float lfophase;
    float SnH_tgt[4];
    float SnH_sum;
    std::array<float, 4> lastKnownPhases;

    static constexpr int LFO_TABLE_SIZE = 8192;
    static constexpr int LFO_TABLE_MASK = LFO_TABLE_SIZE - 1;
    float sin_lfo_table[LFO_TABLE_SIZE]{};

    const SIMD_M128 LFO_TABLE_SIZE_SSE = SIMD_MM(set_ps)(
        (float)LFO_TABLE_SIZE, (float)LFO_TABLE_SIZE, (float)LFO_TABLE_SIZE, (float)LFO_TABLE_SIZE);
    const SIMD_M128I LFO_TABLE_MASK_SSE =
        SIMD_MM(set_epi32)(LFO_TABLE_MASK, LFO_TABLE_MASK, LFO_TABLE_MASK, LFO_TABLE_MASK);

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
