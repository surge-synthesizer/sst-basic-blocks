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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_ADSRENVELOPE_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_ADSRENVELOPE_H

#include <cmath>
#include <cassert>
#include <algorithm>
#include "sst/basic-blocks/simd/setup.h"
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
struct ADSREnvelope : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>
{
    using base_t = DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>;

    SRProvider *srProvider;
    ADSREnvelope(SRProvider *s) : DiscreteStagesEnvelope<BLOCK_SIZE, RangeProvider>(), srProvider(s)
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
        this->stage = base_t::s_attack;
        isDigital = isdig;

        v_c1 = f;
        v_c1_delayed = f;
        discharge = false;

        base_t::resetCurrent();
    }

    float rFrom{0};
    inline float targetDigitalADSR(const float a, const float d, const float s, const float r,
                                   const int ashape, const int dshape, const int rshape,
                                   const bool gateActive)
    {
        auto &stage = this->stage;

        if (!gateActive && stage < base_t::s_release)
        {
            rFrom = this->output;

            switch (rshape)
            {
            case 0:
                rFrom = rFrom * rFrom;
                break;
            case 2:
                rFrom = pow(rFrom, 1.0 / 3.0);
                break;
            }

            stage = base_t::s_release;
            phase = 0;
        }
        switch (stage)
        {
        case base_t::s_attack:
        {
            phase += srProvider->envelope_rate_linear_nowrap(a * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                phase = 0;
                stage = base_t::s_decay;
                return 1;
            }
            return phase;
        }
        break;

        case base_t::s_decay:
        {
            phase += srProvider->envelope_rate_linear_nowrap(d * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                phase = 0;
                stage = base_t::s_sustain;
                return s;
            }
            auto S = s;
            switch (dshape)
            {
            case 0:
                S = S * S;
                break;
            case 2:
                S = pow(S, 1.0 / 3.0);
                break;
            }

            auto dNorm = 1 - phase;
            dNorm = (dNorm) * (1.0 - S) + S;
            // FIXME - deal with shapes
            return dNorm;
        }
        break;

        case base_t::s_sustain:
        {
            return s;
        }
        break;
        case base_t::s_release:
        {
            phase += srProvider->envelope_rate_linear_nowrap(r * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                phase = 0;
                stage = base_t::s_eoc;
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                return 0;
            }
            return rFrom * (1 - phase);
        }
        break;
        default:
            assert(false);
            return 0;
        }
        return 0;
    }

    inline float targetAnalogADSR(const float a, const float d, const float s, const float r,
                                  const int ashape, const int dshape, const int rshape,
                                  const bool gateActive)
    {
        auto &stage = this->stage;

        float coef_A =
            powf(2.f, std::min(0.f, coeff_offset - (a * base_t::etScale + base_t::etMin)));
        float coef_D =
            powf(2.f, std::min(0.f, coeff_offset - (d * base_t::etScale + base_t::etMin)));
        float coef_R =
            (stage >= base_t::s_eoc)
                ? 6.f
                : pow(2.f, std::min(0.f, coeff_offset - (r * base_t::etScale + base_t::etMin)));

        const float v_cc = 1.01f;
        float v_gate = gateActive ? v_cc : 0.f;

        // discharge = SIMD_MM(and_ps)(SIMD_MM(or_ps)(SIMD_MM(cmpgt_ss)(v_c1_delayed, one),
        // discharge), v_gate);
        discharge = ((v_c1_delayed >= 1) || discharge) && gateActive;
        v_c1_delayed = v_c1;

        if (stage == base_t::s_attack)
        {
            phase += srProvider->envelope_rate_linear_nowrap(a * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                stage = base_t::s_decay;
                phase = 0;
                discharge = true;
            }
        }

        float sparm = std::clamp(s, 0.f, 1.f);
        float S = sparm; // * sparm;
        switch (dshape)
        {
        case 0:
            S = S * S;
            break;
        case 2:
            S = pow(S, 1.0 / 3.0);
            break;
        }

        float v_attack = discharge ? 0 : v_gate;
        float v_decay = discharge ? S : v_cc;
        float v_release = v_gate;

        float diff_v_a = std::max(0.f, v_attack - v_c1);
        float diff_v_d = (discharge && gateActive) ? v_decay - v_c1 : std::min(0.f, v_decay - v_c1);
        float diff_v_r = std::min(0.f, v_release - v_c1);

        v_c1 = v_c1 + diff_v_a * coef_A;
        v_c1 = v_c1 + diff_v_d * coef_D;
        v_c1 = v_c1 + diff_v_r * coef_R;

        if (stage <= base_t::s_decay && !gateActive)
        {
            auto backoutShape = (stage == base_t::s_decay ? dshape : ashape);
            // Back out the D scaling
            switch (backoutShape)
            {
            case 0:
                v_c1 = sqrt(v_c1);
                break;
            case 2:
                v_c1 = v_c1 * v_c1 * v_c1;
                break;
            }

            stage = base_t::s_release;

            // put in the R scaling
            switch (rshape)
            {
            case 0:
                v_c1 = v_c1 * v_c1;
                break;
            case 2:
                v_c1 = pow(v_c1, 1.0 / 3.0); // fixme faster glitch
                break;
            }
            phase = 0;
        }

        if (stage == base_t::s_release)
        {
            phase += srProvider->envelope_rate_linear_nowrap(r * base_t::etScale + base_t::etMin);
            if (phase > 1)
            {
                stage = base_t::s_analog_residual_release;
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
            }
        }

        auto res = v_c1;
        if (!gateActive && !discharge && v_c1 < 1e-6)
        {
            if (stage != base_t::s_analog_residual_release)
            {
                res = 0;
                this->eoc_countdown = (int)std::round(srProvider->samplerate * 0.01);
                stage = base_t::s_eoc;
            }
            else
            {
                stage = base_t::s_complete;
                this->eoc_countdown = 0;
            }
        }

        return res;
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
                target = targetDigitalADSR(a, d, s, r, ashape, dshape, rshape, gateActive);
            else
                target = targetAnalogADSR(a, d, s, r, 1, 1, 1, gateActive);

            if (isDigital)
            {
                target = base_t::shapeTarget(target, ashape, dshape, rshape);
            }

            base_t::updateBlockTo(target);
        }

        base_t::step();
    }

    inline void processBlock(const float a, const float d, const float s, const float r,
                             const int ashape, const int dshape, const int rshape,
                             const bool gateActive)
    {
        this->current = BLOCK_SIZE;
        process(a, d, s, r, ashape, dshape, rshape, gateActive);
        if (this->stage == base_t::s_complete || this->stage == base_t::s_eoc)
        {
            memset(this->outputCache, 0, sizeof(this->outputCache));
        }
    }
};
} // namespace sst::basic_blocks::modulators
#endif // RACK_HACK_ADARENVELOPE_H
