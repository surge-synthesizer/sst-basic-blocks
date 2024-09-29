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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_AHDSRSHAPEDSC_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_AHDSRSHAPEDSC_H

#include "DiscreteStagesEnvelope.h"
#include "../tables/TwoToTheXProvider.h"
#include <cassert>
#include <iostream>
#include <memory>

namespace sst::basic_blocks::modulators
{
template <typename SRProvider, int BLOCK_SIZE, typename RangeProvider = TenSecondRange>
struct AHDSRShapedSC : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    static constexpr int nTables{64};
    static constexpr int nLUTPoints{256};
    static thread_local inline float lut[nTables][nLUTPoints];
    static thread_local inline bool lutsInitialized{false};

    static constexpr size_t expLutSize{1024};
    static thread_local inline float expLut[expLutSize];

    SRProvider *srProvider{nullptr};
    AHDSRShapedSC(SRProvider *s)
        : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>(), srProvider(s)
    {
        assert(srProvider);
        initializeLuts();
    }

    static void initializeLuts()
    {
        if (lutsInitialized)
            return;

        if constexpr (RangeProvider::phaseStrategy == ENVTIME_EXP)
        {
            for (int i = 0; i < expLutSize; ++i)
            {
                double x = 1.0 * i / (expLutSize - 1);
                auto timeInSeconds =
                    (std::exp(RangeProvider::A + x * (RangeProvider::B - RangeProvider::A)) +
                     RangeProvider::C) /
                    RangeProvider::D;
                auto invTime = 1.0 / timeInSeconds;
                expLut[i] = std::log2(invTime);
            }
        }

        lutsInitialized = true;
    }

    inline void attackFrom(float fv, bool skipAttack = false)
    {
        if (skipAttack)
        {
            phase = 0;
            attackStartValue = fv;
            this->outBlock0 = 1.0;
            this->stage = base_t::s_hold;
        }
        else
        {
            phase = 0;
            attackStartValue = fv;
            this->stage = base_t::s_attack;
        }
    }

    inline float dPhase(float x)
    {
        if constexpr (RangeProvider::phaseStrategy == DPhaseStrategies::ENVTIME_2TWOX)
        {
            return srProvider->envelope_rate_linear_nowrap(x * base_t::etScale + base_t::etMin);
        }

        if constexpr (RangeProvider::phaseStrategy == ENVTIME_EXP)
        {
            if (x == 0.0)
                return 1.0;

            static thread_local tables::TwoToTheXProvider twoToX;
            if (!twoToX.isInit)
                twoToX.init();

#if CHECK_VS_LUT
            auto timeInSeconds =
                (std::exp(RangeProvider::A + x * (RangeProvider::B - RangeProvider::A)) -
                 RangeProvider::C) /
                RangeProvider::D;

            auto checkV = 1.0 / timeInSeconds;
#endif
            auto xp = std::clamp((double)x, 0., 0.9999999999) * (expLutSize - 1);
            int xpi = (int)xp;
            auto xpf = xp - xpi;
            auto interp = (1 - xpf) * expLut[xpi] + xpf * expLut[xpi + 1];
            auto res = twoToX.twoToThe(interp);

#if CHECK_VS_LUT
            static float lastX = -23;
            if (x != lastX)
            {
                std::cout << "Checkint at x=" << x << "\n  calc=" << checkV << " lut=" << res
                          << "\n  time=" << timeInSeconds << "\n  sr=" << srProvider->sampleRate
                          << " sri=" << srProvider->sampleRateInv
                          << " 1/sri=" << 1.0 / srProvider->sampleRateInv << " bs=" << BLOCK_SIZE
                          << "\n  dPhaseLUT=" << BLOCK_SIZE * srProvider->sampleRateInv * res
                          << "\n  dPhaseCAL=" << BLOCK_SIZE * srProvider->sampleRateInv * checkV
                          << "\n  1/dPhaseLUT="
                          << 1 / (BLOCK_SIZE * srProvider->sampleRateInv * res)
                          << "\n  1/dPhaseCAL="
                          << 1 / (BLOCK_SIZE * srProvider->sampleRateInv * checkV) << std::endl;
                lastX = x;
            }

#endif

            auto dPhase = BLOCK_SIZE * srProvider->sampleRateInv * res;
            return dPhase;
        }
    }
    // from https://martin.ankerl.com/2012/01/25/optimized-approximative-pow-in-c-and-cpp/
    // also check out https://www.hxa.name/articles/content/fast-pow-adjustable_hxa7241_2007.html
    inline double fastPow(double a, double b)
    {
        static_assert(sizeof(double) == 2 * sizeof(int32_t));
        union
        {
            double d;
            int32_t x[2];
        } u = {a};
        u.x[1] = (int)(b * (u.x[1] - 1072632447) + 1072632447);
        u.x[0] = 0;
        return u.d;
    }

    // Shape is -1,1; phase is 0,1
    inline float kernel(float p, float shape)
    {
        auto fshape = std::fabs(shape);
        if (fshape < 1e-4)
        {
            /*
             * e^ax-1 -> 1 + ax + (ax)^2/2 + ...
             * so ax + (ax^2)/2 / ( a + a^2/2)
             * but since we square shape anyway we can just
             * drop the second order term here, avoid the divide by zero, and
             */
            return phase;
        }

        // TODO: We probably want a LUT or other approximation here
        constexpr float scale{8.f};
        // Square it for better response
        auto scsh = scale * shape * fshape;
        return (std::exp(scsh * p) - 1) / (std::exp(scsh) - 1);
    }

    inline void processCore(const float a, const float h, const float d, const float s,
                            const float r, const float ashape, const float dshape,
                            const float rshape, const bool gateActive, bool needsCurve)
    {
        float target = 0;

        auto &stage = this->stage;

        if (!gateActive && stage < base_t::s_release)
        {
            if (r == 0)
            {
                stage = base_t::s_complete;
            }
            else
            {
                stage = base_t::s_release;
                releaseStartValue = this->outputCache[0];
                phase = 0;
            }
        }

        // Accelerate traversal through the state machien for the 0 case
        if (a == 0.f && stage == base_t::s_attack)
        {
            phase = 0;
            stage = base_t::s_hold;
        }
        if (h == 0.f && stage == base_t::s_hold)
        {
            phase = 0.f;
            stage = base_t::s_decay;
        }
        if (d == 0.f && stage == base_t::s_decay)
        {
            phase = 0.f;
            stage = base_t::s_sustain;
        }

        /*
         * We can simply inline this here since there's no switches really
         */
        switch (stage)
        {
        case base_t::s_attack:
        {
            phase += dPhase(a);
            if (phase > 1)
            {
                if (h > 0)
                {
                    stage = base_t::s_hold;
                }
                else
                {
                    stage = base_t::s_decay;
                }
                phase -= 1.f;
                target = 1.0;
            }
            else
            {
                target = kernel(phase, ashape) * (1 - attackStartValue) + attackStartValue;
            }
        }
        break;
        case base_t::s_hold:
        {
            phase += dPhase(h);

            target = 1;
            if (phase > 1)
            {
                stage = base_t::s_decay;
                phase -= 1.f;
            }
        }
        break;
        case base_t::s_decay:
        {
            phase += dPhase(d);
            if (phase > 1)
            {
                stage = base_t::s_sustain;
                target = s;
            }
            else
            {
                target = (1.0 - kernel(phase, dshape)) * (1.0 - s) + s;
            }
        }
        break;
        case base_t::s_sustain:
        {
            target = s;
        }
        break;
        case base_t::s_release:
        {
            phase += dPhase(r);
            if (phase > 1)
            {
                stage = base_t::s_complete;
                target = 0;
            }
            else
            {
                target = (1 - kernel(phase, rshape)) * releaseStartValue;
            }
        }
        break;
        default:
            target = 0.f;
            break;
        }

        if (needsCurve)
        {
            base_t::updateBlockToNoCube(target);
        }
        else
        {
            this->outBlock0 = target;
            this->current = 0;
        }
    }

    inline void process(const float a, const float h, const float d, const float s, const float r,
                        const float ashape, const float dshape, const float rshape,
                        const bool gateActive)
    {
        assert(lutsInitialized);
        if (base_t::preBlockCheck())
            return;

        if (this->current == BLOCK_SIZE)
        {
            processCore(a, h, d, s, r, ashape, rshape, dshape, gateActive, true);
        }

        base_t::step();
    }

    inline void processBlock(const float a, const float h, const float d, const float s,
                             const float r, const float ashape, const float dshape,
                             const float rshape, const bool gateActive, bool needsCurve)
    {
        processCore(a, h, d, s, r, ashape, dshape, rshape, gateActive, needsCurve);
    }

    float phase{0.f}, attackStartValue{0.f}, releaseStartValue{0.f};
};
}; // namespace sst::basic_blocks::modulators

#endif // SHORTCIRCUIT_AHDSRSHAPEDLUT_H
