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

#ifndef INCLUDE_SST_BASIC_BLOCKS_MODULATORS_STEPLFO_H
#define INCLUDE_SST_BASIC_BLOCKS_MODULATORS_STEPLFO_H

#include <array>
#include "Transport.h"
#include "sst/basic-blocks/dsp/RNG.h"
#include "sst/basic-blocks/tables/EqualTuningProvider.h"

namespace sst::basic_blocks::modulators
{
template <size_t blockSize> struct StepLFO
{
    sst::basic_blocks::tables::EqualTuningProvider &tuning;

    struct Storage
    {
        static constexpr int stepLfoSteps{32};

        Storage() { std::fill(data.begin(), data.end(), 0.f); }
        std::array<float, stepLfoSteps> data;
        int16_t repeat{16};

        float smooth{0.f};
        bool rateIsForSingleStep{false};
    };

    StepLFO(sst::basic_blocks::tables::EqualTuningProvider &t) : tuning(t) {}

    void assign(Storage *s, float rate, sst::basic_blocks::modulators::Transport *td,
                sst::basic_blocks::dsp::RNG &rng, bool tempoSync)
    {
        tuning.init();
        this->storage = s;
        this->td = td;
        state = 0;
        output = 0.f;
        phase = 0.;
        ratemult = 1.f;
        shuffle_id = 0;

        // if (settings->triggerMode == 2)
        if (false)
        {
            // simulate free running lfo by randomizing start phase
            // TODO: Use songpos for freerun properly maybe?
            phase = rng.unif01();
            state = rng.unifInt(0, storage->repeat - 1);
        }
        else if (false) // if (settings->triggerMode == 1)
        {
            double ipart; //,tsrate = localcopy[rate].f;
            phase = (float)modf(0.5f * td->timeInBeats * pow(2.0, (double)rate), &ipart);
            int i = (int)ipart;
            state = i % storage->repeat;
        }

        // move state one step ahead to reflect the lag in the interpolation
        state = (state + 1) % storage->repeat;
        for (int i = 0; i < 4; i++)
            wf_history[i] = storage->data[((state + storage->repeat - i) % storage->repeat) & 0x1f];

        UpdatePhaseIncrement(rate, tempoSync);
    }

    float lrate{-10000};
    double tsVal{0.f};
    void UpdatePhaseIncrement(float rate, bool tempoSync)
    {
        if (tempoSync && td)
        {
            // Temposync rates change less often and need full
            // double precision to avoid drift fo things like
            // 5 minute voices and the like.
            if (lrate != rate)
            {
                tsVal = pow(2.0, rate);
            }
            lrate = rate;
            phaseInc = blockSize * tsVal * samplerate_inv *
                       (storage->rateIsForSingleStep ? 1 : storage->repeat) * td->tempo *
                       (1.0 / 120.);
        }
        else
        {
            phaseInc = blockSize * tuning.note_to_pitch(12 * rate) * samplerate_inv *
                       (storage->rateIsForSingleStep ? 1 : storage->repeat);
        }
    }

    void setSampleRate(double sr, double sri)
    {
        samplerate = sr;
        samplerate_inv = sri;
    }

    void retrigger()
    {
        state = 0;
        phase = 0;

        // Again a 1 step lag in interpolation
        state = (state + 1) % storage->repeat;
        for (int i = 0; i < 4; i++)
            wf_history[i] = storage->data[((state + storage->repeat - i) % storage->repeat) & 0x1f];
    }

    void process(float rate, int triggerMode, bool ts, bool oneShot, int samples)
    {
        phase += phaseInc;
        while (phase > 1.0)
        {
            state++;

            if (oneShot)
            {
                state = std::min((long)(storage->repeat - 1), state);
            }
            else if (state >= storage->repeat)
            {
                state = 0;
            }
            phase -= 1.0;

            wf_history[3] = wf_history[2];
            wf_history[2] = wf_history[1];
            wf_history[1] = wf_history[0];
            wf_history[0] = storage->data[state & (Storage::stepLfoSteps - 1)];
        }

        UpdatePhaseIncrement(rate, ts);
        output = std::clamp(lfo_ipol(wf_history, phase, storage->smooth, state & 1), -1.f, 1.f);
    }

    float QuadraticBSpline(float y0, float y1, float y2, float mu)
    {
        return 0.5f *
               (y2 * (mu * mu) + y1 * (-2 * mu * mu + 2 * mu + 1) + y0 * (mu * mu - 2 * mu + 1));
    }
    float lfo_ipol(float *wf_history, float phase, float smooth, int odd)
    {
        float df = smooth * 0.5f;
        float iout;

        if (df > 0.5f)
        {
            float linear;

            if (phase > 0.5f)
            {
                float ph = phase - 0.5f;
                linear = (1.f - ph) * wf_history[1] + ph * wf_history[0];
            }
            else
            {
                float ph = phase + 0.5f;
                linear = (1.f - ph) * wf_history[2] + ph * wf_history[1];
            }

            float qbs = QuadraticBSpline(wf_history[2], wf_history[1], wf_history[0], phase);

            iout = (2.f - 2.f * df) * linear + (2.f * df - 1.0f) * qbs;
        }
        else if (df > -0.0001f)
        {
            if (phase > 0.5f)
            {
                float cf = 0.5f - (phase - 1.f) / (2.f * df + 0.00001f);
                cf = std::max(0.f, std::min(cf, 1.0f));
                iout = (1.f - cf) * wf_history[0] + cf * wf_history[1];
            }
            else
            {
                float cf = 0.5f - (phase) / (2.f * df + 0.00001f);
                cf = std::max(0.f, std::min(cf, 1.0f));
                iout = (1.f - cf) * wf_history[1] + cf * wf_history[2];
            }
        }
        else if (df > -0.5f)
        {
            float cf = std::max(0.f, std::min((1.f - phase) / (-2.f * df + 0.00001f), 1.0f));
            iout = (1.f - cf) * 0 + cf * wf_history[1];
        }
        else
        {
            float cf = std::max(0.f, std::min(phase / (2.f + 2.f * df + 0.00001f), 1.0f));
            iout = (1.f - cf) * wf_history[1] + cf * 0;
        }
        return iout;
    }

    float output{0.f};

    double phase{0};

    long getCurrentStep() const
    {
        auto res = state - 1;
        if (res < 0)
            res = storage->repeat - 1;

        return res;
    }

  protected:
    long state;
    long state_tminus1;
    double phaseInc;
    float wf_history[4];
    float ratemult;
    int shuffle_id;
    sst::basic_blocks::modulators::Transport *td{nullptr};
    Storage *storage{nullptr};
    float priorRate{-1000};

    double samplerate{1}, samplerate_inv{1};
};
} // namespace sst::basic_blocks::modulators
#endif // STEPLFO_H
