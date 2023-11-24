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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_ADARENVELOPE_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_ADARENVELOPE_H

#include <algorithm>
#include <cmath>
#include <cassert>
#include "DiscreteStagesEnvelope.h"

namespace sst::basic_blocks::modulators
{
/**
 * ADAREnvelope is an attack decay or a gated attack release envelope, the only
 * difference being a hold-while-gated approach.
 *
 * @tparam SRProvider See the comments in ADSREnvelope
 * @tparam BLOCK_SIZE Must be a power of 2
 * @tparam RangeProvider Defines the min and max
 */
template <typename SRProvider, int BLOCK_SIZE, typename RangeProvider = TenSecondRange>
struct ADAREnvelope : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    SRProvider *srProvider;
    ADAREnvelope(SRProvider *s) : srProvider(s) {}

    bool isDigital{true};
    bool isGated{false};

    float phase{0}, start{0};

    // Analog Mode
    float v_c1{0}, v_c1_delayed{0.f};
    bool discharge{false};

    void attackFrom(float fv, int ashp, bool id, bool ig)
    {
        float f = fv;

        if (id)
        {
            switch (ashp)
            {
            case 0:
                // target = sqrt(target);
                f = f * f;
                break;
            case 2:
                // target = target * target * target;
                f = pow(f, 1.0 / 3.0);
                break;
            }
        }
        phase = f;
        this->stage = base_t::s_attack;
        isDigital = id;
        isGated = ig;

        v_c1 = f;
        v_c1_delayed = f;
        discharge = false;

        base_t::resetCurrent();
    }

    template <bool gated>
    inline float stepDigital(const float a, const float d, const bool gateActive)
    {
        float target = 0;
        switch (this->stage)
        {
        default:
            break;
        case base_t::s_attack:
        {
            phase += srProvider->envelope_rate_linear_nowrap(a);
            if (phase >= 1)
            {
                phase = 1;
                if constexpr (gated)
                    this->stage = base_t::s_hold;
                else
                    this->stage = base_t::s_decay;
            }
            if constexpr (gated)
                if (!gateActive)
                    this->stage = base_t::s_decay;
            target = phase;
            break;
        }
        case base_t::s_decay:
        {
            phase -= srProvider->envelope_rate_linear_nowrap(d);
            if (phase <= 0)
            {
                phase = 0;
                this->stage = base_t::s_eoc;
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
            }
            target = phase;
        }
        }
        return target;
    }

    inline void process01AD(const float a, const float d, const int ashape, const int dshape,
                            const bool gateActive)
    {
        processScaledAD(this->rateFrom01(1), this->rateFrom01(d), ashape, dshape, gateActive);
    }

    inline void processScaledAD(const float a, const float d, const int ashape, const int dshape,
                                const bool gateActive)
    {
        if (base_t::preBlockCheck())
            return;

        if (this->current == BLOCK_SIZE)
        {
            float target = 0;
            if (isGated && this->stage == base_t::s_hold)
            {
                target = 1;

                if (!gateActive)
                {
                    phase = 1;
                    this->stage = base_t::s_decay;
                }
            }
            else if (isDigital)
            {
                if (isGated)
                    target = stepDigital<true>(a, d, gateActive);
                else
                    target = stepDigital<false>(a, d, gateActive);
            }
            else
            {
                const float coeff_offset =
                    2.f - std::log2(srProvider->samplerate * base_t::BLOCK_SIZE_INV);

                auto ndc = (v_c1_delayed >= 0.99999f);
                if (ndc && !discharge)
                {
                    phase = 1;
                    this->stage = isGated ? base_t::s_hold : base_t::s_decay;
                }

                if (isGated && !discharge)
                {
                    ndc = !gateActive;
                }
                discharge = ndc || discharge;
                v_c1_delayed = v_c1;

                static constexpr float v_gate = 1.02f;
                auto v_attack = (!discharge) * v_gate;
                auto v_decay = (!discharge) * v_gate;

                // In this case we only need the coefs in their this->stage
                float coef_A = !discharge ? powf(2.f, std::min(0.f, coeff_offset - a)) : 0;
                float coef_D = discharge ? powf(2.f, std::min(0.f, coeff_offset - d)) : 0;

                auto diff_v_a = std::max(0.f, v_attack - v_c1);
                auto diff_v_d = std::min(0.f, v_decay - v_c1);

                v_c1 = v_c1 + diff_v_a * coef_A + diff_v_d * coef_D;
                target = v_c1;

                if (this->stage == base_t::s_decay)
                {
                    phase -= srProvider->envelope_rate_linear_nowrap(d);
                    if (phase <= 0)
                    {
                        this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                        this->stage = base_t::s_analog_residual_decay;
                    }
                }
                if (v_c1 < 1e-6 && discharge)
                {
                    v_c1 = 0;
                    v_c1_delayed = 0;
                    discharge = false;
                    target = 0;
                    if (this->stage != base_t::s_analog_residual_decay)
                    {
                        this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                        this->stage = base_t::s_eoc;
                    }
                    else
                    {
                        this->stage = base_t::s_complete;
                        this->eoc_countdown = 0;
                    }
                }
            }

            if (isDigital)
            {
                target = base_t::shapeTarget(target, ashape, dshape);
            }

            base_t::updateBlockTo(target);
        }

        base_t::step();
    }
};
} // namespace sst::basic_blocks::modulators
#endif // RACK_HACK_ADARENVELOPE_H
