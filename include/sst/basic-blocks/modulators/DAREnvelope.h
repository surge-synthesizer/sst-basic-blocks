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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DARENVELOPE_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DARENVELOPE_H

#include <algorithm>
#include <cmath>
#include <cassert>
#include "DiscreteStagesEnvelope.h"

namespace sst::basic_blocks::modulators
{
/**
 * DAREnvelope is an gated delay attack sustain-at-one release envelope
 *
 * @tparam SRProvider See the comments in ADSREnvelope
 * @tparam BLOCK_SIZE Must be a power of 2
 * @tparam RangeProvider Defines the min and max
 */
template <typename SRProvider, int BLOCK_SIZE, typename RangeProvider = TenSecondRange>
struct DAREnvelope : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    SRProvider *srProvider;
    DAREnvelope(SRProvider *s) : srProvider(s) {}

    float phase{0}, start{0};

    void attack(const float d)
    {
        phase = 0;
        if (d > 0.f)
        {
            this->stage = base_t::s_delay;
        }
        else
        {
            this->stage = base_t::s_attack;
        }

        base_t::resetCurrent();
    }

    // The value here is in natural units by now
    inline float dPhase(float x)
    {
        if constexpr (RangeProvider::phaseStrategy == DPhaseStrategies::ENVTIME_2TWOX)
        {
            return srProvider->envelope_rate_linear_nowrap(x);
        }

        if constexpr (RangeProvider::phaseStrategy == ENVTIME_EXP)
        {
            auto timeInSeconds =
                (std::exp(RangeProvider::A + x * (RangeProvider::B - RangeProvider::A)) -
                 RangeProvider::C) /
                RangeProvider::D;
            auto dPhase = BLOCK_SIZE * srProvider->sampleRateInv / timeInSeconds;

            return dPhase;
        }
    }

    template <bool gated> inline float stepDigital(const float d, const float a, const float r)
    {
        float target = 0;
        switch (this->stage)
        {
        case base_t::s_delay:
        {
            phase += dPhase(d);
            if (phase >= 1)
            {
                if constexpr (gated)
                {
                    phase -= 1.0;
                    this->stage = base_t::s_attack;
                }
                else
                {
                    this->stage = base_t::s_eoc;
                    this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                }
            }
        }
        break;
        case base_t::s_attack:
        {
            phase += dPhase(a);
            if (phase >= 1)
            {
                phase = 1;
                if constexpr (gated)
                {
                    this->stage = base_t::s_hold;
                }
                else
                {
                    this->stage = base_t::s_release;
                }
            }
            if constexpr (!gated)
            {
                this->stage = base_t::s_release;
            }
            target = phase;
            break;
        }
        case base_t::s_hold:
        {
            target = 1;
            if constexpr (!gated)
            {
                this->stage = base_t::s_release;
                phase = 1;
            }
        }
        break;
        case base_t::s_release:
        {
            phase -= dPhase(r);
            if (phase <= 0)
            {
                phase = 0;
                this->stage = base_t::s_eoc;
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
            }
            target = phase;
        }
        break;

        default:
            break;
        }
        return target;
    }

    inline void processBlock01AD(const float d, const float a, const float r, const bool gateActive)
    {
        processBlockScaledAD(this->rateFrom01(d), this->rateFrom01(a), this->rateFrom01(r),
                             gateActive);
    }

    inline void processBlockScaledAD(const float d, const float a, const float r,
                                     const bool gateActive)
    {
        if (base_t::preBlockCheck())
            return;

        float target = 0;

        if (gateActive)
            target = stepDigital<true>(d, a, r);
        else
            target = stepDigital<false>(d, a, r);

        base_t::updateBlockTo(target);
        base_t::step();
    }
    inline void processScaledAD(const float d, const float a, const float r, const bool gateActive)
    {
        if (base_t::preBlockCheck())
            return;

        if (this->current == BLOCK_SIZE)
        {
            float target = 0;

            if (gateActive)
                target = stepDigital<true>(d, a, r);
            else
                target = stepDigital<false>(d, a, r);

            base_t::updateBlockTo(target);
        }

        base_t::step();
    }
};
} // namespace sst::basic_blocks::modulators
#endif // RACK_HACK_ADARENVELOPE_H
