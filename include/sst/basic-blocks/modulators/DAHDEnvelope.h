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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DAHDENVELOPE_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_DAHDENVELOPE_H

#include <cmath>
#include <cassert>
#include "DiscreteStagesEnvelope.h"

namespace sst::basic_blocks::modulators
{

/**
 * The ADSR or DAHD envelope provider
 *
 * @tparam SRProvider - a class which provides two functions. ->samplerate is the current
 * sample rate and envelope_rate_linear_nowrap which provides an envelope rate. The
 * rate is BLOCK_SIZE * 2^-v / samplerate but is usually provided by a table lookup, like
 * in SurgeStorage
 *
 * @tparam BLOCK_SIZE  the block size
 * @tparam RangeProvider - sets mins and maxes
 */
template <typename SRProvider, int BLOCK_SIZE, typename RangeProvider = TenSecondRange>
struct DAHDEnvelope : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    SRProvider *srProvider;
    DAHDEnvelope(SRProvider *s) : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>(), srProvider(s)
    {
        onSampleRateChanged();
    }

    bool isDigital{true};
    float phase{0}, start{0};

    // Analog Mode
    float v_c1{0}, v_c1_delayed{0.f};
    bool discharge{false};

    float coeff_offset{0};
    void onSampleRateChanged()
    {
        coeff_offset = 2.f - std::log2(srProvider->samplerate * base_t::BLOCK_SIZE_INV);
    }

    void attackFrom(float fv, float attack, int ashp, bool isdig)
    {
        float f = fv;

        if (isdig)
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
        if (attack > 0.0001)
        {
            this->stage = base_t::s_delay;
        }
        else
        {
            this->stage = base_t::s_attack;
        }
        isDigital = isdig;

        v_c1 = f;
        v_c1_delayed = f;
        discharge = false;

        base_t::resetCurrent();
    }

    float rFrom{0};
    inline float targetDigitalDAHD(const float dly, const float a, const float h, const float d,
                                   const int ashape, const int dshape, const int rshape)
    {
        auto &stage = this->stage;

        switch (stage)
        {
        case base_t::s_delay:
        {
            phase += srProvider->envelope_rate_linear_nowrap(dly * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                stage = base_t::s_attack;
                phase -= 1;
                return phase;
            }
            return 0;
        }
        break;
        case base_t::s_attack:
        {
            phase += srProvider->envelope_rate_linear_nowrap(a * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                phase = 0;
                stage = base_t::s_sustain;
                return 1;
            }
            return phase;
        }
        break;
        case base_t::s_sustain:
        {
            phase += srProvider->envelope_rate_linear_nowrap(h * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                phase = 0;
                stage = base_t::s_release;
                return 1 - phase;
            }
            return 1;
        }
        break;
        case base_t::s_release:
        {
            phase += srProvider->envelope_rate_linear_nowrap(d * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                phase = 0;
                stage = base_t::s_eoc;
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                return 0;
            }
            return (1 - phase);
        }
        break;
        default:
            assert(false);
            return 0;
        }
        return 0;
    }

    inline float targetAnalogDAHD(const float dly, const float a, const float h, const float d,
                                  const int ashape, const int dshape, const int rshape)
    {
        auto &stage = this->stage;

        if (stage == base_t::s_delay)
        {
            phase += srProvider->envelope_rate_linear_nowrap(dly * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                stage = base_t::s_attack;
                phase -= 1;
            }
            return 0;
        }

        if (stage == base_t::s_sustain)
        {
            phase += srProvider->envelope_rate_linear_nowrap(h * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                stage = base_t::s_release;
                phase = 1;
            }
            return 1;
        }

        auto ndc = (v_c1_delayed >= 0.99999f);
        if (ndc && !discharge)
        {
            phase = 0;
            stage = base_t::s_sustain;
        }

        discharge = ndc || discharge;
        v_c1_delayed = v_c1;

        static constexpr float v_gate = 1.02f;
        auto v_attack = (!discharge) * v_gate;
        auto v_decay = (!discharge) * v_gate;

        // In this case we only need the coefs in their stage
        float coef_A =
            !discharge
                ? powf(2.f, std::min(0.f, coeff_offset - (a * base_t::etScale + base_t::etMin)))
                : 0;
        float coef_D =
            discharge
                ? powf(2.f, std::min(0.f, coeff_offset - (d * base_t::etScale + base_t::etMin)))
                : 0;

        auto diff_v_a = std::max(0.f, v_attack - v_c1);
        auto diff_v_d = std::min(0.f, v_decay - v_c1);

        v_c1 = v_c1 + diff_v_a * coef_A + diff_v_d * coef_D;
        auto res = v_c1;

        if (stage == base_t::s_release)
        {
            phase -= srProvider->envelope_rate_linear_nowrap(d * base_t::etScale + base_t::etMin);
            if (phase <= 0)
            {
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                stage = base_t::s_analog_residual_release;
            }
        }
        if (v_c1 < 1e-6 && discharge)
        {
            v_c1 = 0;
            v_c1_delayed = 0;
            discharge = false;
            res = 0;
            if (stage != base_t::s_analog_residual_release)
            {
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                stage = base_t::s_eoc;
            }
            else
            {
                stage = base_t::s_complete;
                this->eoc_countdown = 0;
            }
        }
        return (stage == base_t::s_sustain ? 1 : res);
    }

    inline void process(const float a, const float d, const float s, const float r,
                        const int ashape, const int dshape, const int rshape, const bool gateActive)
    {
        if (base_t::preBlockCheck())
            return;

        if (this->current == BLOCK_SIZE)
        {
            float target = 0;

            if (isDigital)
                target = targetDigitalDAHD(a, d, s, r, ashape, dshape, rshape);
            else
                target = targetAnalogDAHD(a, d, s, r, 1, 1, 1);

            if (isDigital)
            {
                target = base_t::shapeTarget(target, ashape, dshape, rshape);
            }

            base_t::updateBlockTo(target);
        }

        base_t::step();
    }
};
} // namespace sst::basic_blocks::modulators
#endif // RACK_HACK_ADARENVELOPE_H
