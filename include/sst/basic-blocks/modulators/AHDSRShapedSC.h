/*
 * sst-basic-blocks - a Surge Synth Team product
 *
 * Provides basic tools and blocks for use on the audio thread in
 * synthesis, including basic DSP and modulation functions
 *
 * Copyright 2019 - 2023, Various authors, as described in the github
 * transaction log.
 *
 * sst-basic-blocks is released under the Gnu General Public Licence
 * V3 or later (GPL-3.0-or-later). The license is found in the file
 * "LICENSE" in the root of this repository or at
 * https://www.gnu.org/licenses/gpl-3.0.en.html
 *
 * All source for sst-basic-blocks is available at
 * https://github.com/surge-synthesizer/sst-basic-blocks
 */

#ifndef SST_BASIC_BLOCKS_MODULATORS_AHDRSSHAPEDLUT_H
#define SST_BASIC_BLOCKS_MODULATORS_AHDRSSHAPEDLUT_H

#include "DiscreteStagesEnvelope.h"
#include <cassert>

namespace sst::basic_blocks::modulators
{
template <typename SRProvider, int BLOCK_SIZE, typename RangeProvider = TenSecondRange>
struct AHDSRShapedLUT : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    static constexpr int nTables{64};
    static constexpr int nLUTPoints{256};
    static inline float lut[nTables][nLUTPoints];
    static inline bool lutsInitialized{false};

    SRProvider *srProvider{nullptr};
    AHDSRShapedLUT(SRProvider *s)
        : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>(), srProvider(s)
    {
        assert(srProvider);
    }

    static void initializeLuts()
    {
        if (lutsInitialized)
            return;

        std::cout << "INITIALIZING LUTS" << std::endl;
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

    inline void process(const float a, const float h, const float d, const float s, const float r,
                        const float ashape, const float dshape, const float rshape,
                        const bool gateActive)
    {
        assert(lutsInitialized);
        if (base_t::preBlockCheck())
            return;

        if (this->current == BLOCK_SIZE)
        {
            float target = 0;

            auto &stage = this->stage;

            if (!gateActive && stage < base_t::s_release)
            {
                stage = base_t::s_release;
                releaseStartValue = this->output;
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
                    target = phase * (1 - attackStartValue) + attackStartValue;
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
                    target = (1.0 - phase) * (1.0 - s) + s;
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
                    target = (1 - phase) * releaseStartValue;
                }
            }
            break;
            default:
                target = 0.f;
                break;
            }

            base_t::updateBlockTo(target);
        }

        base_t::step();
    }

    inline void processBlock(const float a, const float h, const float d, const float s,
                             const float r, const float ashape, const float dshape,
                             const float rshape, const bool gateActive)
    {
        this->current = BLOCK_SIZE;
        process(a, h, d, s, r, ashape, dshape, rshape, gateActive);
        if (this->stage == base_t::s_complete || this->stage == base_t::s_eoc)
        {
            memset(this->outputCache, 0, sizeof(this->outputCache));
        }
    }

    float phase{0.f}, attackStartValue{0.f}, releaseStartValue{0.f};
};
}; // namespace sst::basic_blocks::modulators

#endif // SHORTCIRCUIT_AHDSRSHAPEDLUT_H
