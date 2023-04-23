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
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source in sst-basic-blocks available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_AHDSRSHAPEDSC_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_AHDSRSHAPEDSC_H

#include "DiscreteStagesEnvelope.h"
#include <cassert>

namespace sst::basic_blocks::modulators
{
template <typename SRProvider, int BLOCK_SIZE, typename RangeProvider = TenSecondRange>
struct AHDSRShapedSC : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    static constexpr int nTables{64};
    static constexpr int nLUTPoints{256};
    static inline float lut[nTables][nLUTPoints];
    static inline bool lutsInitialized{false};

    SRProvider *srProvider{nullptr};
    AHDSRShapedSC(SRProvider *s)
        : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>(), srProvider(s)
    {
        assert(srProvider);
    }

    static void initializeLuts()
    {
        if (lutsInitialized)
            return;

        lutsInitialized = true;
    }

    inline void attackFrom(float fv)
    {
        phase = 0;
        attackStartValue = fv;
        this->stage = base_t::s_attack;
    }

    inline float dPhase(float x)
    {
        return srProvider->envelope_rate_linear_nowrap(x * base_t::etScale + base_t::etMin);
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
    inline float kernel(float phase, float shape)
    {
        auto b = -shape * 8.f;
        auto c = (b < 0 ? (1.f / (1 - b) - 1) : b);
        auto r = std::pow(phase, (1.f + c));
        return r;
    }

    inline void processCore(const float a, const float h, const float d, const float s,
                            const float r, const float ashape, const float dshape,
                            const float rshape, const bool gateActive)
    {
        float target = 0;

        auto &stage = this->stage;

        if (!gateActive && stage < base_t::s_release)
        {
            stage = base_t::s_release;
            releaseStartValue = this->outputCache[0];
            phase = 0;
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
                if (h > base_t::etMin)
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

        base_t::updateBlockTo(target);
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
            processCore(a, h, d, s, r, ashape, rshape, dshape, gateActive);
        }

        base_t::step();
    }

    inline void processBlock(const float a, const float h, const float d, const float s,
                             const float r, const float ashape, const float dshape,
                             const float rshape, const bool gateActive)
    {
        processCore(a, h, d, s, r, ashape, dshape, rshape, gateActive);
    }

    float phase{0.f}, attackStartValue{0.f}, releaseStartValue{0.f};
};
}; // namespace sst::basic_blocks::modulators

#endif // SHORTCIRCUIT_AHDSRSHAPEDLUT_H
