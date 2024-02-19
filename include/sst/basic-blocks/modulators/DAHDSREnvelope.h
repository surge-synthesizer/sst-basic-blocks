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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DAHDSRENVELOPE_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DAHDSRENVELOPE_H

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
template <typename SRProvider, int BLOCK_SIZE, typename RangeProvider = TenSecondRange,
          bool processEverySample = true>
struct DAHDSREnvelope : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    SRProvider *srProvider;
    DAHDSREnvelope(SRProvider *s) : srProvider(s) {}

    float phase{0}, start{0}, releaseScale{1.f};

    void attack(const float d)
    {
        phase = 0;
        releaseScale = 1.f;
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

    template <bool gated>
    inline float stepDigital(const float dl, const float a, const float h, const float dc,
                             const float s, const float r)
    {
        float target = 0;
        switch (this->stage)
        {
        case base_t::s_delay:
        {
            phase += srProvider->envelope_rate_linear_nowrap(dl);
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
            phase += srProvider->envelope_rate_linear_nowrap(a);
            target = phase;
            if constexpr (!gated)
            {
                this->stage = base_t::s_release;
            }
            else if (phase >= 1)
            {
                target = 1;
                this->stage = base_t::s_hold;
                phase -= 1;
            }

            break;
        }
        case base_t::s_hold:
        {
            phase += srProvider->envelope_rate_linear_nowrap(h);

            target = 1;
            if constexpr (!gated)
            {
                this->stage = base_t::s_release;
                phase = 1;
            }
            else if (phase >= 1)
            {
                this->stage = base_t::s_decay;
                phase -= 1;
            }
        }
        break;
        case base_t::s_decay:
        {
            phase += srProvider->envelope_rate_linear_nowrap(dc);
            target = (1 - phase) * (1 - s) + s;
            if constexpr (!gated)
            {
                this->stage = base_t::s_release;
                phase = 1;
                releaseScale = target;
            }
            else if (phase >= 1)
            {
                phase = 1;
                target = s;
                this->stage = base_t::s_sustain;
            }
        }
        break;
        case base_t::s_sustain:
        {
            target = s;
            if constexpr (!gated)
            {
                this->stage = base_t::s_release;
                releaseScale = s;
                phase = 1;
            }
        }
        break;
        case base_t::s_release:
        {
            phase -= srProvider->envelope_rate_linear_nowrap(r);
            if (phase <= 0)
            {
                phase = 0;
                this->stage = base_t::s_eoc;
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
            }
            target = phase * releaseScale;
        }
        break;

        default:
            break;
        }
        return target;
    }

    inline void processBlock01AD(const float dl, const float a, const float h, const float dc,
                                 const float s, const float r, const bool gateActive)
    {
        processBlockScaledAD(this->rateFrom01(dl), this->rateFrom01(a), this->rateFrom01(h),
                             this->rateFrom01(dc), s, this->rateFrom01(r), gateActive);
    }

    inline void processBlockScaledAD(const float dl, const float a, const float h, const float dc,
                                     const float s, const float r, const bool gateActive)
    {
        if (base_t::preBlockCheck())
            return;

        float target = 0;

        if (gateActive)
            target = stepDigital<true>(dl, a, h, dc, s, r);
        else
            target = stepDigital<false>(dl, a, h, dc, s, r);

        base_t::updateBlockTo(target);
        base_t::step();
    }
    inline void processScaledAD(const float dl, const float a, const float h, const float dc,
                                const float s, const float r, const bool gateActive)
    {
        if (base_t::preBlockCheck())
            return;

        if (!processEverySample || this->current == BLOCK_SIZE)
        {
            float target = 0;

            if (gateActive)
                target = stepDigital<true>(dl, a, h, dc, s, r);
            else
                target = stepDigital<false>(dl, a, h, dc, s, r);

            base_t::updateBlockTo(target);
        }

        base_t::step();
    }
};
} // namespace sst::basic_blocks::modulators
#endif // RACK_HACK_ADARENVELOPE_H
